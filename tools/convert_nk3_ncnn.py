#!/usr/bin/env python3
"""Convert TenRiff's NK3 ONNX models into ncnn deployment models.

The Vulkan graph keeps P64's floating-point inference work. Exact integer
pressure bookkeeping and the boolean validity mask are intentionally evaluated
by the TenRiff host because ncnn tensors do not preserve the source graph's
BOOL/INT64 contract. This script removes those host-owned branches, replaces
the remaining boolean relation helpers with equivalent 0/1 float operations,
and converts the result with pnnx.
"""

from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path

import onnx
from onnx import TensorProto, helper, numpy_helper


def _replacement_nodes(node: onnx.NodeProto) -> list[onnx.NodeProto] | None:
    """Replace boolean-only ONNX ops with equivalent float 0/1 operations."""
    a = node.input[0] if node.input else ""
    b = node.input[1] if len(node.input) > 1 else ""
    output = node.output[0]
    stem = f"ncnn_{node.name or output}"

    if node.op_type in {"Less", "Greater", "GreaterOrEqual", "Equal"}:
        difference = f"{stem}_difference"
        signed = f"{stem}_signed"
        indicator = f"{stem}_indicator"
        if node.op_type == "Less":
            subtract_inputs = [b, a]
        elif node.op_type == "Greater":
            subtract_inputs = [a, b]
        elif node.op_type == "GreaterOrEqual":
            subtract_inputs = [b, a]
        else:
            subtract_inputs = [a, b]
        result = [
            helper.make_node("Sub", subtract_inputs, [difference], name=f"{stem}_sub"),
        ]
        if node.op_type == "Equal":
            absolute = f"{stem}_absolute"
            result.append(helper.make_node("Abs", [difference], [absolute], name=f"{stem}_abs"))
            result.append(helper.make_node("Sign", [absolute], [signed], name=f"{stem}_sign"))
            result.append(helper.make_node("Sub", ["one_f", signed], [output], name=f"{stem}_result"))
            return result
        result.append(helper.make_node("Sign", [difference], [signed], name=f"{stem}_sign"))
        result.append(helper.make_node("Max", [signed, "zero_f"], [indicator], name=f"{stem}_positive"))
        if node.op_type == "GreaterOrEqual":
            result.append(helper.make_node("Sub", ["one_f", indicator], [output], name=f"{stem}_result"))
        else:
            result.append(helper.make_node("Identity", [indicator], [output], name=f"{stem}_result"))
        return result

    if node.op_type == "And":
        return [helper.make_node("Mul", [a, b], [output], name=f"{stem}_mul")]
    if node.op_type == "Or":
        return [helper.make_node("Max", [a, b], [output], name=f"{stem}_max")]
    if node.op_type == "Not":
        return [helper.make_node("Sub", ["one_f", a], [output], name=f"{stem}_sub")]
    if node.op_type == "Where":
        positive = f"{stem}_positive"
        inverse = f"{stem}_inverse"
        negative = f"{stem}_negative"
        return [
            helper.make_node("Mul", [a, b], [positive], name=f"{stem}_mul_positive"),
            helper.make_node("Sub", ["one_f", a], [inverse], name=f"{stem}_inverse_mask"),
            helper.make_node("Mul", [inverse, node.input[2]], [negative], name=f"{stem}_mul_negative"),
            helper.make_node("Add", [positive, negative], [output], name=f"{stem}_add"),
        ]
    if node.op_type == "Cast":
        return [helper.make_node("Identity", [a], [output], name=f"{stem}_identity")]
    return None


def prepare_p64_vulkan_graph(source: Path, destination: Path) -> None:
    model = onnx.load(source)
    graph = model.graph
    for value in graph.input:
        if value.name in {"mask", "memory_mask"}:
            value.type.tensor_type.elem_type = TensorProto.FLOAT
    graph.input.append(
        helper.make_tensor_value_info("addition_count_f", TensorProto.FLOAT, [1, 64])
    )
    if not any(initializer.name == "one_f" for initializer in graph.initializer):
        graph.initializer.append(
            numpy_helper.from_array(__import__("numpy").array(1.0, dtype="float32"), "one_f")
        )

    rewritten = []
    for node in graph.node:
        if node.name == "CastAdditionCount":
            continue
        replacement = _replacement_nodes(node)
        rewritten.extend(replacement if replacement is not None else [node])
    del graph.node[:]
    graph.node.extend(rewritten)

    model = onnx.shape_inference.infer_shapes(model)
    onnx.checker.check_model(model)
    with tempfile.TemporaryDirectory(prefix="tenriff-nk3-extract-") as scratch:
        full_graph = Path(scratch) / "p64-float.onnx"
        onnx.save(model, full_graph)
        onnx.utils.extract_model(
            str(full_graph),
            str(destination),
            [
                "notes",
                "mask",
                "parameters",
                "fast_context_state",
                "slow_context_state",
                "memory",
                "memory_mask",
                "addition_count_f",
            ],
            [
                "candidate_scores_rounded",
                "next_fast_context",
                "next_slow_context",
                "next_memory",
            ],
            check_model=True,
        )


def prepare_pattern_vulkan_graph(source: Path, destination: Path) -> None:
    """Expose the two unique role logits before ONNX repeats them eight times."""
    model = onnx.load(source)
    batch_rows = model.graph.input[0].type.tensor_type.shape.dim[0].dim_value
    del model.graph.output[:]
    model.graph.output.append(
        helper.make_tensor_value_info(
            "bounded_roles", TensorProto.FLOAT, [batch_rows, 2]
        )
    )
    model = onnx.shape_inference.infer_shapes(model)
    onnx.checker.check_model(model)
    onnx.save(model, destination)


def patch_reductions(
    param_path: Path, *, axis: int, reduce_max_keepdims: bool
) -> None:
    """Restore ONNX reduction axes that pnnx omits for opset-18 inputs."""
    lines = param_path.read_text(encoding="utf-8").splitlines()
    patched = []
    reduction_count = 0
    reduce_max_count = 0
    for line in lines:
        fields = line.split()
        if fields and fields[0] in {"Reduction", "ReduceMax"}:
            is_reduce_max = fields[0] == "ReduceMax"
            if is_reduce_max:
                fields[0] = "Reduction"
                reduce_max_count += 1
            fields = [field for field in fields if not field.startswith("-23303=")]
            if is_reduce_max:
                fields.extend(
                    ["0=4", "1=0", f"4={int(reduce_max_keepdims)}", "5=1"]
                )
            fields.append(f"-23303=1,{axis}")
            line = " ".join(fields)
            reduction_count += 1
        patched.append(line)
    if reduce_max_count != 1:
        raise RuntimeError(
            f"expected one ReduceMax layer in {param_path}, found {reduce_max_count}"
        )
    if reduction_count == 0:
        raise RuntimeError(f"expected Reduction layers in {param_path}")
    forbidden = ("Tensor.to", "torch.", "pnnx.Expression", "TopK", "CumSum", "Mod", "ReduceMax")
    custom = sorted({line.split()[0] for line in patched[2:] if line.split() and line.split()[0].startswith(forbidden)})
    if custom:
        raise RuntimeError(f"unsupported ncnn layers remain in {param_path}: {custom}")
    param_path.write_text("\n".join(patched) + "\n", encoding="utf-8")


def run_pnnx(
    pnnx: Path,
    model: Path,
    output_stem: Path,
    *,
    reduction_axis: int,
    reduce_max_keepdims: bool,
) -> None:
    output_stem.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tenriff-pnnx-") as scratch:
        scratch_path = Path(scratch)
        command = [
            str(pnnx),
            str(model),
            f"ncnnparam={output_stem}.param",
            f"ncnnbin={output_stem}.bin",
            f"pnnxparam={scratch_path / 'model.pnnx.param'}",
            f"pnnxbin={scratch_path / 'model.pnnx.bin'}",
            f"pnnxpy={scratch_path / 'model_pnnx.py'}",
            f"pnnxonnx={scratch_path / 'model.pnnx.onnx'}",
            f"ncnnpy={scratch_path / 'model_ncnn.py'}",
            "fp16=0",
            "optlevel=2",
        ]
        subprocess.run(command, check=True)
    patch_reductions(
        Path(f"{output_stem}.param"),
        axis=reduction_axis,
        reduce_max_keepdims=reduce_max_keepdims,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pnnx", type=Path, required=True)
    parser.add_argument("--models-dir", type=Path, default=Path("models"))
    parser.add_argument("--output-dir", type=Path, default=Path("models/ncnn"))
    args = parser.parse_args()

    pnnx = args.pnnx.resolve()
    models_dir = args.models_dir.resolve()
    output_dir = args.output_dir.resolve()
    if not pnnx.is_file():
        raise FileNotFoundError(f"pnnx executable not found: {pnnx}")

    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tenriff-nk3-") as scratch:
        wrapped_p64 = Path(scratch) / "NK3-P64-hybrid-ncnn.onnx"
        prepare_p64_vulkan_graph(models_dir / "NK3-P64-hybrid.onnx", wrapped_p64)
        run_pnnx(
            pnnx,
            wrapped_p64,
            output_dir / "NK3-P64-hybrid.ncnn",
            reduction_axis=2,
            reduce_max_keepdims=True,
        )

    with tempfile.TemporaryDirectory(prefix="tenriff-nk3-pattern-") as scratch:
        scratch_path = Path(scratch)
        for target_keys in range(2, 19):
            filename = f"NK3-general-pattern-{target_keys}K.onnx"
            wrapped_pattern = scratch_path / filename
            prepare_pattern_vulkan_graph(models_dir / filename, wrapped_pattern)
            run_pnnx(
                pnnx,
                wrapped_pattern,
                output_dir / f"NK3-general-pattern-{target_keys}K.ncnn",
                reduction_axis=1,
                reduce_max_keepdims=False,
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
