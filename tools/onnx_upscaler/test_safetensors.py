"""Offline adapter checks; all weights are temporary structural fixtures."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock

sys.dont_write_bytecode = True
import numpy as np
import onnx
import onnxruntime as ort
import torch
from safetensors.torch import save_file

spec = importlib.util.spec_from_file_location("converter", Path(__file__).with_name("convert_safetensors.py"))
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)


def weights(training=False):
    generator = torch.Generator().manual_seed(16)
    state = {}
    def convolution(name, output, inputs, kernel):
        state[name + ".weight"] = torch.randn(output, inputs, kernel, kernel, generator=generator) * .005
        state[name + ".bias"] = torch.randn(output, generator=generator) * .003
    convolution("stem", 32, 3, 3)
    for block in range(8):
        for layer in ("first", "second"):
            name = f"blocks.{block}.{layer}"
            if training:
                convolution(name + ".spatial", 32, 32, 3)
                convolution(name + ".pointwise", 32, 32, 1)
            else:
                convolution(name, 32, 32, 3)
    convolution("trunk", 32, 32, 3)
    convolution("head", 12, 32, 3)
    return state


def reference(state, x, training=False, gain=.1):
    from torch.nn import functional as f
    def conv(name, value):
        weight = state[name + ".weight"]
        return f.conv2d(value, weight, state[name + ".bias"], padding=weight.shape[-1] // 2)
    shallow = conv("stem", x)
    value = shallow
    for block in range(8):
        residual = value
        for layer in ("first", "second"):
            name = f"blocks.{block}.{layer}"
            residual = (conv(name + ".spatial", residual) + conv(name + ".pointwise", residual) + residual) if training else conv(name, residual)
            if layer == "first":
                residual = f.relu(residual)
        value = value + residual * (gain if training else 1)
    return f.pixel_shuffle(conv("head", shallow + conv("trunk", value)), 2)


class ConversionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="tenriff safetensors 한글 ")
        self.root = Path(self.directory.name)
        self.source = self.root / "model & quoted.safetensors"
        self.cache = self.root / "cache"

    def tearDown(self):
        self.directory.cleanup()

    def test_deployed_dynamic_pixels_metadata_and_cache(self):
        state = weights()
        save_file(state, self.source)
        target = converter.convert(self.source, self.cache)
        graph = onnx.load(target)
        self.assertEqual({p.key: p.value for p in graph.metadata_props}["tenriff.output_mode"], "residual")
        session = ort.InferenceSession(str(target), providers=["CPUExecutionProvider"])
        for height, width in [(1, 1), (5, 13), (17, 9)]:
            data = torch.linspace(0, 1, 3 * height * width).reshape(1, 3, height, width)
            actual = session.run(None, {"rgb_lr": data.numpy()})[0]
            np.testing.assert_allclose(actual, reference(state, data).numpy(), atol=2e-6, rtol=1e-4)
        stamp = target.stat().st_mtime_ns
        self.assertEqual(converter.convert(self.source, self.cache), target)
        self.assertEqual(stamp, target.stat().st_mtime_ns)
        target.write_bytes(b"corrupt cache")
        self.assertEqual(converter.convert(self.source, self.cache), target)
        onnx.checker.check_model(onnx.load(target))

    def test_training_gain_is_preserved_and_required(self):
        state = weights(True)
        save_file(state, self.source, metadata={"lunasr2.residual_gain": "0.125"})
        target = converter.convert(self.source, self.cache)
        session = ort.InferenceSession(str(target), providers=["CPUExecutionProvider"])
        data = torch.rand(1, 3, 9, 11)
        np.testing.assert_allclose(session.run(None, {"rgb_lr": data.numpy()})[0], reference(state, data, True, .125).numpy(), atol=2e-6, rtol=1e-4)
        with self.assertRaisesRegex(ValueError, "residual_gain"):
            converter.luna_model(state, {})
        with self.assertRaisesRegex(ValueError, "gain"):
            converter.luna_model(state, {"lunasr2.residual_gain": "nan"})

    def test_weight_edit_changes_cache_identity(self):
        state = weights()
        save_file(state, self.source)
        first = converter.convert(self.source, self.cache)
        state["head.bias"][0] += .02
        save_file(state, self.source)
        second = converter.convert(self.source, self.cache)
        self.assertNotEqual(first, second)
        self.assertTrue(first.is_file())

    def test_rejects_unknown_architecture_without_spandrel(self):
        with mock.patch.dict(sys.modules, {"spandrel": None}):
            with self.assertRaisesRegex(ValueError, "Spandrel"):
                converter.select_model({"unknown": torch.ones(1)}, {})

    def test_rejects_incomplete_luna_and_nonfinite(self):
        state = weights()
        del state["trunk.bias"]
        with self.assertRaises(RuntimeError):
            converter.luna_model(state, {})
        state = weights()
        state["head.bias"][0] = float("nan")
        save_file(state, self.source)
        with self.assertRaisesRegex(ValueError, "non-finite"):
            converter.load_tensors(self.source)

    def test_pickle_extensions_and_invalid_safetensors_are_never_loaded(self):
        source = self.root / "unsafe.pth"
        source.write_bytes(b"not a tensor file")
        with self.assertRaisesRegex(ValueError, "safetensors"):
            converter.convert(source, self.cache)
        self.source.write_bytes(b"not a tensor file")
        with self.assertRaises(Exception):
            converter.load_tensors(self.source)

    def test_missing_dependencies_gives_actionable_error(self):
        with mock.patch.object(converter.importlib.metadata, "version", side_effect=converter.importlib.metadata.PackageNotFoundError):
            with self.assertRaisesRegex(ValueError, "no packages are installed automatically"):
                converter.dependencies()

    def test_caller_temporary_path_is_removed(self):
        save_file(weights(), self.source)
        self.cache.mkdir()
        temporary = self.cache / "owned.onnx.tmp"
        converter.convert(self.source, self.cache, temporary)
        self.assertFalse(temporary.exists())
        state = weights()
        state["head.bias"][0] += .05
        save_file(state, self.source)
        with mock.patch.object(converter, "verify_export", side_effect=ValueError("verification failed")):
            with self.assertRaisesRegex(ValueError, "verification failed"):
                converter.convert(self.source, self.cache, temporary)
        self.assertFalse(temporary.exists())

    def test_spandrel_adapter_preserves_descriptor_completed_image_contract(self):
        # A synthetic stand-in verifies our descriptor/export boundary. It is not
        # evidence that an absent Spandrel package or a real architecture works.
        class Descriptor:
            input_channels = output_channels = 3
            scale = 2
            tiling = 1
            architecture = types.SimpleNamespace(id="synthetic-descriptor")
            model = torch.nn.Upsample(scale_factor=2, mode="nearest")
            def eval(self): return self
            def float(self): return self
            def __call__(self, image): return self.model(image).clamp(.1, .9)
        descriptor = Descriptor()
        package = types.SimpleNamespace(ImageModelDescriptor=Descriptor,
            ModelLoader=lambda **kwargs: types.SimpleNamespace(load_from_state_dict=lambda state: descriptor),
            ModelTiling=types.SimpleNamespace(SUPPORTED=1, DISCOURAGED=2, INTERNAL=3))
        save_file({"synthetic": torch.ones(1)}, self.source)
        with mock.patch.dict(sys.modules, {"spandrel": package}):
            target = converter.convert(self.source, self.cache)
            props = {p.key: p.value for p in onnx.load(target).metadata_props}
            self.assertEqual(props["tenriff.output_mode"], "image")
            session = ort.InferenceSession(str(target), providers=["CPUExecutionProvider"])
            self.assertEqual(session.get_inputs()[0].shape, [1,3,256,256])
            for value in (0., 1.):
                actual = session.run(None, {"rgb_lr": np.full((1,3,256,256),value,np.float32)})[0]
                np.testing.assert_array_equal(actual, np.full((1,3,512,512),np.clip(value,.1,.9),np.float32))
            descriptor.input_channels = 1
            with self.assertRaisesRegex(ValueError, "RGB"):
                converter.select_model({}, {})
            descriptor.input_channels = 3
            descriptor.tiling = 2
            with self.assertRaisesRegex(ValueError, "fixed-tile"):
                converter.select_model({}, {})


if __name__ == "__main__":
    torch.set_num_threads(2)
    unittest.main()
