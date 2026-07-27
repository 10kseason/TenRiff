#!/usr/bin/env python3
"""Export a LunaSR Dense8-B6 checkpoint to TenRiff's fixed WinML contract."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
import torch
from torch import nn


class ResidualBlock(nn.Module):
    def __init__(self, channels: int) -> None:
        super().__init__()
        self.conv = nn.Conv2d(channels, channels, 3, padding=1)

    def forward(self, value: torch.Tensor) -> torch.Tensor:
        return torch.relu(value + self.conv(value))


class LunaSrResidual(nn.Module):
    def __init__(self, channels: int, blocks: int) -> None:
        super().__init__()
        self.head = nn.Conv2d(1, channels, 3, padding=1)
        self.body = nn.ModuleList(ResidualBlock(channels) for _ in range(blocks))
        self.tail = nn.Conv2d(channels, 4, 3, padding=1)

    def forward(self, luma_lr: torch.Tensor) -> torch.Tensor:
        value = torch.relu(self.head(luma_lr))
        for block in self.body:
            value = block(value)
        return torch.pixel_shuffle(self.tail(value), 2)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def clear_doc_strings(message: object) -> None:
    """Remove exporter diagnostics that can contain machine-local source paths."""
    descriptor = getattr(message, "DESCRIPTOR", None)
    if descriptor is None:
        return
    for field in descriptor.fields:
        if field.name == "doc_string":
            setattr(message, field.name, "")
            continue
        if field.message_type is None:
            continue
        value = getattr(message, field.name)
        if field.is_repeated:
            for item in value:
                clear_doc_strings(item)
        elif message.HasField(field.name):
            clear_doc_strings(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--verification", type=Path)
    args = parser.parse_args()

    checkpoint = torch.load(args.checkpoint, map_location="cpu", weights_only=False)
    config = checkpoint.get("config", {})
    channels = int(config.get("channels", 8))
    blocks = int(config.get("blocks", 6))
    if channels != 8 or blocks != 6 or config.get("block_kind", "dense") != "dense":
        raise ValueError(f"unsupported LunaSR checkpoint config: {config!r}")

    model = LunaSrResidual(channels, blocks).eval()
    model.load_state_dict(checkpoint["model"], strict=True)
    dummy = torch.zeros((1, 1, 540, 960), dtype=torch.float32)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    torch.onnx.export(
        model,
        dummy,
        args.output,
        input_names=["luma_lr"],
        output_names=["luma_residual"],
        opset_version=18,
        do_constant_folding=True,
        dynamo=False,
    )

    exported = onnx.load(args.output)
    exported.ir_version = 9
    exported.producer_name = "TenRiff LunaSR"
    exported.producer_version = "basic_v2"
    exported.model_version = 2
    del exported.metadata_props[:]
    clear_doc_strings(exported)
    onnx.checker.check_model(exported)
    onnx.save(exported, args.output)

    rng = np.random.default_rng(20260727)
    sample = rng.random((1, 1, 540, 960), dtype=np.float32)
    with torch.inference_mode():
        expected = model(torch.from_numpy(sample)).cpu().numpy()
    session = ort.InferenceSession(str(args.output), providers=["CPUExecutionProvider"])
    actual = session.run(["luma_residual"], {"luma_lr": sample})[0]
    max_abs_error = float(np.max(np.abs(expected - actual)))
    mean_abs_error = float(np.mean(np.abs(expected - actual)))
    if actual.shape != (1, 1, 1080, 1920):
        raise RuntimeError(f"unexpected ONNX output shape: {actual.shape!r}")
    if not np.allclose(expected, actual, atol=1e-5, rtol=1e-4):
        raise RuntimeError(f"PyTorch/ONNX mismatch: max_abs_error={max_abs_error}")

    verification_path = args.verification or args.output.with_suffix(".verification.json")
    verification = {
        "source_checkpoint": args.checkpoint.name,
        "source_checkpoint_sha256": sha256(args.checkpoint),
        "source_epoch": int(checkpoint.get("epoch", -1)),
        "validation_psnr": float(checkpoint.get("validation_psnr", 0.0)),
        "baseline_psnr": float(checkpoint.get("baseline_psnr", 0.0)),
        "psnr_delta": float(checkpoint.get("psnr_delta", 0.0)),
        "onnx_checker": "passed",
        "onnxruntime": "passed",
        "input_shape": [1, 1, 540, 960],
        "output_shape": [1, 1, 1080, 1920],
        "max_abs_error": max_abs_error,
        "mean_abs_error": mean_abs_error,
        "allclose_atol_1e-5_rtol_1e-4": True,
        "onnx_sha256": sha256(args.output),
    }
    verification_path.write_text(json.dumps(verification, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(verification, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
