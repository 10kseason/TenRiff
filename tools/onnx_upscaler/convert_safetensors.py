"""Optional offline Safetensors -> verified ONNX adapter. No weights are bundled.

Only tensor data is read from the selected file. Architecture code comes from this
script or the user's installed Spandrel package, never from beside a model.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import math
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
MAX_FILE_BYTES = 1024 * 1024 * 1024
EXPORT_VERSION = "tenriff-safetensors-v1"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def dependencies():
    names = ("torch", "safetensors", "onnx", "onnxruntime")
    versions = {}
    for name in names:
        try:
            versions[name] = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            raise ValueError("Safetensors conversion needs Python packages: torch, safetensors, onnx, onnxruntime; no packages are installed automatically.") from None
    try:
        versions["spandrel"] = importlib.metadata.version("spandrel")
    except importlib.metadata.PackageNotFoundError:
        versions["spandrel"] = "not-installed"
    return versions


def load_tensors(path):
    import torch
    from safetensors import safe_open
    from safetensors.torch import load_file

    if path.suffix.lower() != ".safetensors":
        raise ValueError("Only .safetensors tensor archives are accepted.")
    if not 0 < path.stat().st_size <= MAX_FILE_BYTES:
        raise ValueError("Safetensors file must be at most 1 GiB.")
    with safe_open(path, framework="pt", device="cpu") as archive:
        metadata = archive.metadata() or {}
    state = load_file(path, device="cpu")
    if not state or len(state) > 10000:
        raise ValueError("Invalid or excessive tensor count.")
    if any(t.dtype not in (torch.float16, torch.float32, torch.bfloat16, torch.int64, torch.int32)
           or not torch.isfinite(t).all() for t in state.values()):
        raise ValueError("Unsupported tensor dtype or non-finite weights.")
    return state, metadata


def luna_model(state, metadata):
    """Exact known C32 residual topology; unknown scalar gains are never guessed."""
    import torch
    from torch import nn
    from torch.nn import functional as functional

    if state and all(key.startswith("core.") for key in state):
        state = {key[5:]: value for key, value in state.items()}
    if not all(key in state for key in ("stem.weight", "trunk.weight", "head.weight")):
        return None
    if tuple(state["stem.weight"].shape) != (32, 3, 3, 3) or tuple(state["head.weight"].shape) != (12, 32, 3, 3):
        return None
    indexes = {int(match[1]) for key in state if (match := re.match(r"blocks\.(\d+)\.", key))}
    blocks = len(indexes)
    if blocks not in (8, 12, 16) or indexes != set(range(blocks)):
        raise ValueError("LunaSR2 requires contiguous C32 R8/R12/R16 x2 blocks.")
    training = "blocks.0.first.spatial.weight" in state
    gain = 1.0
    if training:
        try:
            gain = float(metadata["lunasr2.residual_gain"])
        except (KeyError, ValueError):
            raise ValueError("Training-form LunaSR2 requires lunasr2.residual_gain metadata; export deployment-form weights or supply the original training gain.") from None
        if not math.isfinite(gain) or not 0 < gain <= 0.25:
            raise ValueError("Unsupported LunaSR2 residual gain.")

    class RepConv(nn.Module):
        def __init__(self):
            super().__init__()
            self.spatial = nn.Conv2d(32, 32, 3, padding=1)
            self.pointwise = nn.Conv2d(32, 32, 1)

        def forward(self, x):
            return self.spatial(x) + self.pointwise(x) + x

    class Block(nn.Module):
        def __init__(self):
            super().__init__()
            self.first = RepConv() if training else nn.Conv2d(32, 32, 3, padding=1)
            self.second = RepConv() if training else nn.Conv2d(32, 32, 3, padding=1)

        def forward(self, x):
            return x + self.second(functional.relu(self.first(x))) * gain

    class Luna(nn.Module):
        def __init__(self):
            super().__init__()
            self.stem = nn.Conv2d(3, 32, 3, padding=1)
            self.blocks = nn.Sequential(*(Block() for _ in range(blocks)))
            self.trunk = nn.Conv2d(32, 32, 3, padding=1)
            self.head = nn.Conv2d(32, 12, 3, padding=1)

        def forward(self, x):
            shallow = self.stem(x)
            return functional.pixel_shuffle(self.head(shallow + self.trunk(self.blocks(shallow))), 2)

    model = Luna().eval()
    model.load_state_dict(state, strict=True)
    return model, "residual", 2, True, f"LunaSR2-C32-R{blocks}-x2"


def select_model(state, metadata):
    luna = luna_model(state, metadata)
    if luna is not None:
        return luna
    try:
        import spandrel
    except ImportError:
        raise ValueError("Unrecognized LunaSR2 weights. Other Safetensors architectures require an installed Spandrel package; compatible ONNX can be selected directly.") from None
    descriptor = spandrel.ModelLoader(device="cpu").load_from_state_dict(state)
    if not isinstance(descriptor, spandrel.ImageModelDescriptor):
        raise ValueError("Only Spandrel image-to-image models are supported.")
    if descriptor.input_channels != 3 or descriptor.output_channels != 3 or descriptor.scale not in (1, 2, 3, 4):
        raise ValueError("Spandrel model must use RGB input/output and integer x1-x4 scale.")
    if descriptor.tiling == spandrel.ModelTiling.DISCOURAGED or descriptor.tiling == spandrel.ModelTiling.INTERNAL:
        raise ValueError("This Safetensors architecture does not support the fixed-tile export path; provide a compatible ONNX instead.")
    descriptor.eval().float()
    from torch import nn

    class ImageAdapter(nn.Module):
        def __init__(self):
            super().__init__()
            self.model = descriptor.model

        def forward(self, image):
            # Preserve Spandrel's documented normalization, padding and crop.
            return descriptor(image)

    return ImageAdapter().eval(), "image", descriptor.scale, False, str(descriptor.architecture.id)


def verify_export(model, target, scale, dynamic):
    import numpy as np
    import onnx
    import onnxruntime as ort
    import torch

    graph = onnx.load(target, load_external_data=False)
    onnx.checker.check_model(graph, full_check=True)
    if any(item.data_location == onnx.TensorProto.EXTERNAL for item in graph.graph.initializer):
        raise ValueError("External-data ONNX exports are unsupported.")
    if len(graph.graph.input) != 1 or len(graph.graph.output) != 1:
        raise ValueError("Export must have exactly one input and one output.")
    options = ort.SessionOptions()
    options.intra_op_num_threads = 2
    options.inter_op_num_threads = 1
    session = ort.InferenceSession(str(target), options, providers=["CPUExecutionProvider"])
    shapes = [(1, 3, 17, 23), (1, 3, 25, 19)] if dynamic else [(1, 3, 256, 256)]
    for shape in shapes:
        data = np.random.default_rng(20261016).random(shape, dtype=np.float32)
        with torch.inference_mode():
            expected = model(torch.from_numpy(data)).numpy()
        actual = session.run(None, {"rgb_lr": data})[0]
        if actual.shape != (1, 3, shape[2] * scale, shape[3] * scale):
            raise ValueError("Export has an unsupported output shape.")
        if not np.isfinite(actual).all():
            raise ValueError("Export produces non-finite values.")
        np.testing.assert_allclose(actual, expected, atol=0.0002, rtol=0.001)
    return graph


def convert(source, cache, temporary_path=None):
    source = Path(source).resolve(strict=True)
    if source.suffix.lower() != ".safetensors" or not 0 < source.stat().st_size <= MAX_FILE_BYTES:
        raise ValueError("Select a .safetensors file of at most 1 GiB.")
    versions = dependencies()
    source_digest = sha256(source)
    identity = {"source": source_digest, "exporter": sha256(__file__), "versions": versions, "version": EXPORT_VERSION}
    key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    cache = Path(cache).resolve()
    cache.mkdir(parents=True, exist_ok=True)
    target, manifest = cache / (key + ".onnx"), cache / (key + ".json")
    if target.is_file() and manifest.is_file():
        try:
            record = json.loads(manifest.read_text(encoding="utf-8"))
            if record["identity"] == identity and record["onnx_sha256"] == sha256(target):
                return target
        except (ValueError, KeyError, OSError):
            pass

    import torch
    import onnx
    torch.set_num_threads(2)
    state, metadata = load_tensors(source)
    model, mode, scale, dynamic, architecture = select_model(state, metadata)
    model = model.cpu().float().eval()
    if temporary_path is None:
        handle, temporary = tempfile.mkstemp(prefix="export-", suffix=".onnx.tmp", dir=cache)
        os.close(handle)
        temporary = Path(temporary)
    else:
        temporary = Path(temporary_path).resolve()
        if temporary.parent != cache or temporary.exists():
            raise ValueError("Temporary export must be a new file in the cache.")
        temporary.touch(exist_ok=False)
    try:
        shape = (1, 3, 19, 27) if dynamic else (1, 3, 256, 256)
        with torch.inference_mode():
            torch.onnx.export(model, torch.zeros(shape), temporary, input_names=["rgb_lr"], output_names=["rgb_output"],
                dynamic_axes={"rgb_lr": {2: "height", 3: "width"}, "rgb_output": {2: "scaled_height", 3: "scaled_width"}} if dynamic else None,
                opset_version=12, dynamo=False, external_data=False, export_params=True, keep_initializers_as_inputs=False)
        graph = verify_export(model, temporary, scale, dynamic)
        onnx.helper.set_model_props(graph, {"tenriff.output_mode": mode, "tenriff.safetensors_architecture": architecture,
                                            "tenriff.safetensors_sha256": source_digest})
        onnx.save(graph, temporary)
        # A model edited while conversion ran must not inherit the earlier cache key.
        if source_digest != sha256(source):
            raise ValueError("Source model changed during conversion; select it again.")
        digest = sha256(temporary)
        os.replace(temporary, target)
        manifest.write_text(json.dumps({"identity": identity, "onnx_sha256": digest, "architecture": architecture,
                                        "output_mode": mode, "scale": scale, "dynamic": dynamic}, indent=2), encoding="utf-8")
        return target
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--error", type=Path, required=True)
    parser.add_argument("--temporary", type=Path)
    args = parser.parse_args()
    try:
        result = convert(args.source, args.cache, args.temporary)
        args.result.write_text(str(result), encoding="utf-8")
        return 0
    except Exception as error:
        # The host reads a bounded local diagnostic instead of inheriting handles.
        message = str(error)[:2048]
        args.error.write_text(message, encoding="utf-8")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
