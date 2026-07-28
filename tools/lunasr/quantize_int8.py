from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Iterator

import numpy as np
import onnx
from PIL import Image
from onnxruntime.quantization import (
    CalibrationDataReader,
    CalibrationMethod,
    QuantFormat,
    QuantType,
    quantize_static,
)

IMAGE_SUFFIXES = {".bmp", ".jpeg", ".jpg", ".png", ".webp"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Quantize the LunaSR staged RGB residual model to static S8S8 QDQ INT8."
    )
    parser.add_argument("--input", type=Path, required=True, help="FP32 ONNX model")
    parser.add_argument("--output", type=Path, required=True, help="INT8 ONNX output")
    parser.add_argument(
        "--calibration-root",
        type=Path,
        action="append",
        required=True,
        help="Image directory; repeat to calibrate across multiple content domains",
    )
    parser.add_argument("--samples-per-root", type=int, default=8)
    parser.add_argument("--report", type=Path)
    return parser.parse_args()


def select_calibration_images(root: Path, count: int) -> list[Path]:
    images = sorted(
        path for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES
    )
    if not images:
        raise ValueError(f"no calibration images found under {root}")
    count = min(count, len(images))
    # Pick the center of evenly spaced bins so large captures are represented
    # without depending on private filenames or a random seed.
    indices = [min(len(images) - 1, int((index + 0.5) * len(images) / count))
               for index in range(count)]
    return [images[index] for index in indices]


def image_tensor(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        rgb = image.convert("RGB").resize((960, 540), Image.Resampling.LANCZOS)
    array = np.asarray(rgb, dtype=np.float32) / 255.0
    return np.transpose(array, (2, 0, 1))[None].copy()


class LunaSrCalibrationReader(CalibrationDataReader):
    def __init__(self, images: list[Path]) -> None:
        self.images = images
        self._iterator: Iterator[dict[str, np.ndarray]] | None = None

    def get_next(self) -> dict[str, np.ndarray] | None:
        if self._iterator is None:
            self._iterator = iter({"rgb_lr": image_tensor(path)} for path in self.images)
        return next(self._iterator, None)

    def rewind(self) -> None:
        self._iterator = None


def main() -> None:
    args = parse_args()
    if args.output.exists():
        raise SystemExit(f"refusing to overwrite output: {args.output}")
    if args.samples_per_root < 1:
        raise SystemExit("--samples-per-root must be at least 1")

    calibration_images: list[Path] = []
    samples_by_root: list[int] = []
    for root in args.calibration_root:
        selected = select_calibration_images(root, args.samples_per_root)
        calibration_images.extend(selected)
        samples_by_root.append(len(selected))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    quantize_static(
        model_input=str(args.input),
        model_output=str(args.output),
        calibration_data_reader=LunaSrCalibrationReader(calibration_images),
        quant_format=QuantFormat.QDQ,
        activation_type=QuantType.QInt8,
        weight_type=QuantType.QInt8,
        per_channel=True,
        reduce_range=False,
        op_types_to_quantize=["Conv"],
        calibrate_method=CalibrationMethod.MinMax,
        extra_options={
            "ActivationSymmetric": True,
            "WeightSymmetric": True,
            "CalibTensorRangeSymmetric": True,
        },
    )

    model = onnx.load(str(args.output))
    model.ir_version = 9
    del model.metadata_props[:]
    for key, value in (
        ("tenriff.quantization", "static_s8s8_qdq"),
        ("tenriff.calibration_domains", str(len(args.calibration_root))),
        ("tenriff.calibration_samples", str(len(calibration_images))),
    ):
        entry = model.metadata_props.add()
        entry.key = key
        entry.value = value
    onnx.checker.check_model(model)
    onnx.save(model, str(args.output))

    operator_counts: dict[str, int] = {}
    for node in model.graph.node:
        operator_counts[node.op_type] = operator_counts.get(node.op_type, 0) + 1
    report = {
        "quantization": "static S8S8 QDQ",
        "activation_dtype": "int8",
        "weight_dtype": "int8",
        "model_boundary_dtype": "float32",
        "per_channel_weights": True,
        "calibration_method": "MinMax symmetric",
        "calibration_domains": len(args.calibration_root),
        "calibration_samples": len(calibration_images),
        "samples_by_domain": samples_by_root,
        "ir_version": model.ir_version,
        "opset": model.opset_import[0].version,
        "operator_counts": operator_counts,
        "output_bytes": args.output.stat().st_size,
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
