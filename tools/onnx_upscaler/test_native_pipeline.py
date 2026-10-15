"""Synthetic pixel-contract test; no trained model or quality/FPS claim.

Usage: python test_native_pipeline.py <onnx_upscaler_winml_smoke.exe> <new-output-dir> <ffmpeg-exe>
Requires numpy, Pillow and onnx. All generated models remain in the output dir.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper
from PIL import Image


def make_model(path, input_half=False, output_half=False, detail=False, dynamic=False,
               input_shape=None, output_shape=None, scale=2):
    dtype = TensorProto.FLOAT16 if input_half else TensorProto.FLOAT
    out_type = TensorProto.FLOAT16 if output_half else TensorProto.FLOAT
    nodes = [helper.make_node('Cast', ['rgb_lr'], ['float_input'], to=TensorProto.FLOAT)]
    initializers = [numpy_helper.from_array(np.array([1, 1, scale, scale], np.float32), 'scale'),
                    numpy_helper.from_array(np.array([], np.float32), 'roi'),
                    numpy_helper.from_array(np.array(0.125 if detail else 0.0, np.float32), 'gain')]
    tensor = 'float_input'
    if detail:
        initializers += [numpy_helper.from_array(np.array([0, 0, 1, 1, 0, 0, 1, 1], np.int64), 'pads'),
                         numpy_helper.from_array(np.full((3, 1, 3, 3), 1 / 9, np.float32), 'weights')]
        nodes += [helper.make_node('Pad', [tensor, 'pads'], ['padded'], mode='edge'),
                  helper.make_node('Conv', ['padded', 'weights'], ['filtered'], group=3, kernel_shape=[3, 3])]
        tensor = 'filtered'
    nodes += [helper.make_node('Mul', [tensor, 'gain'], ['detail']),
              helper.make_node('Resize', ['detail', 'roi', 'scale'], ['expanded'],
                               mode='nearest', coordinate_transformation_mode='asymmetric', nearest_mode='floor'),
              helper.make_node('Cast', ['expanded'], ['rgb_residual_x2'], to=out_type)]
    if input_shape is None:
        input_shape = [1, 3, 'height', 'width'] if dynamic else [1, 3, 540, 960]
    if output_shape is None:
        output_shape = [1, 3, 'height_x2', 'width_x2'] if dynamic else [1, 3, 1080, 1920]
    graph = helper.make_graph(nodes, 'native-pixel-fixture',
        [helper.make_tensor_value_info('rgb_lr', dtype, input_shape)],
        [helper.make_tensor_value_info('rgb_residual_x2', out_type, output_shape)], initializers)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 11)], ir_version=7)
    onnx.checker.check_model(model)
    onnx.save(model, path)


def fixture(width, height):
    y, x = np.indices((height, width))
    return np.stack(((x * 11 + y * 3) % 251, (x * 7 + y * 13) % 251,
                     (x // 5 * 33 + y // 3 * 19) % 251, np.where((x + y) % 5, 255, 128)), -1).astype(np.uint8)


def expected_x2(rgba, detail):
    """Full-image reference with no tile planning or fixed model dimensions."""
    height, width = rgba.shape[:2]
    x = (np.arange(width * 2, dtype=np.float64) + 0.5) / 2 - 0.5
    y = (np.arange(height * 2, dtype=np.float64) + 0.5) / 2 - 0.5
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - x0)[None, :, None], (y - y0)[:, None, None]
    xa, xb = np.clip(x0, 0, width - 1), np.clip(x0 + 1, 0, width - 1)
    ya, yb = np.clip(y0, 0, height - 1), np.clip(y0 + 1, 0, height - 1)
    source = rgba.astype(np.float64) / 255
    top = source[ya[:, None], xa] * (1 - fx) + source[ya[:, None], xb] * fx
    bottom = source[yb[:, None], xa] * (1 - fx) + source[yb[:, None], xb] * fx
    base = top * (1 - fy) + bottom * fy
    if detail:
        padded = np.pad(source[:, :, :3], ((1, 1), (1, 1), (0, 0)), mode='edge')
        residual = sum(padded[dy:dy + height, dx:dx + width] for dy in range(3) for dx in range(3)) / 72
        residual = np.repeat(np.repeat(residual, 2, 0), 2, 1)
        base[:, :, :3] = np.clip(base[:, :, :3] + residual, 0, 1)
    base[:, :, :3] *= base[:, :, 3:4]
    return np.floor(base[:, :, [2, 1, 0, 3]] * 255 + 0.5).astype(np.uint8)


def main():
    exe = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2]).resolve()
    ffmpeg = sys.argv[3] if len(sys.argv) > 3 else shutil.which("ffmpeg")
    assert ffmpeg, "Pass the FFmpeg executable as the third argument"
    out.mkdir(parents=True, exist_ok=False)
    env = {k.upper(): v for k, v in os.environ.items()}
    models = [('detail', False, False, True, False), ('fp32', False, False, False, False),
              ('fp16', True, True, False, False), ('mixed', False, True, False, False),
              ('dynamic-detail', False, False, True, True),
              ('dynamic-fp16', True, True, False, True),
              ('dynamic-mixed', False, True, False, True)]
    for name, ih, oh, detail, dynamic in models:
        make_model(out / (name + '.onnx'), ih, oh, detail, dynamic)
    cases = [('square', 256, 256, 'detail'), ('four-three', 640, 480, 'detail'),
             ('portrait', 300, 900, 'detail'), ('tile-seams', 1003, 601, 'detail'),
             ('fhd', 1920, 1080, 'detail'), ('zero', 13, 9, 'fp32'),
             ('half', 17, 11, 'fp16'), ('mixed', 19, 13, 'mixed'),
             ('dynamic-odd', 1003, 601, 'dynamic-detail'),
             ('dynamic-portrait', 173, 401, 'dynamic-detail'),
             ('dynamic-fhd', 1920, 1080, 'dynamic-detail'),
             ('dynamic-half', 17, 11, 'dynamic-fp16'),
             ('dynamic-mixed', 19, 13, 'dynamic-mixed')]
    reports = []
    for name, width, height, model in cases:
        rgba = fixture(width, height)
        image_path = out / (name + '.png')
        Image.fromarray(rgba).save(image_path)
        prefix = out / (name + '-restored')
        with (out / (name + '.log')).open('x', encoding='utf-8') as log:
            result = subprocess.run([str(exe), str(out / (model + '.onnx')), str(image_path), str(prefix)],
                cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
        assert result.returncode == 0, (name, result.returncode)
        shape = json.loads(prefix.with_suffix('.json').read_text())
        assert shape == {'width': width * 2, 'height': height * 2}, (name, shape)
        actual = np.fromfile(prefix.with_suffix('.bgra'), dtype=np.uint8).reshape(height * 2, width * 2, 4)
        filtered = subprocess.run([str(ffmpeg), '-v', 'error', '-threads', '1', '-filter_threads', '1',
            '-f', 'rawvideo', '-pixel_format', 'rgba', '-video_size', f'{width}x{height}',
            '-i', 'pipe:0', '-vf', 'format=gbrp,deblock=filter=weak:block=8:planes=7,format=rgba',
            '-frames:v', '1', '-threads', '1', '-f', 'rawvideo', 'pipe:1'],
            input=rgba.tobytes(), capture_output=True, check=True, timeout=15)
        deblocked = np.frombuffer(filtered.stdout, dtype=np.uint8).reshape(rgba.shape).copy()
        deblocked[:, :, 3] = rgba[:, :, 3]
        expected = expected_x2(deblocked, model.endswith('detail'))
        error = np.abs(actual.astype(np.int16) - expected.astype(np.int16))
        maximum = int(error.max())
        # GPU floating-point sums may round one 8-bit step either side of a tie.
        assert maximum <= 1, (name, maximum, np.unravel_index(error.argmax(), error.shape))
        row = dict(case=name, source=[width, height], restored=[width * 2, height * 2],
                   checked_channels=int(error.size), max_byte_error=maximum, deblock_changed_channels=int(np.count_nonzero(deblocked != rgba)), passed=True)
        reports.append(row)
        (out / 'summary.json').write_text(json.dumps(reports, indent=2), encoding='utf-8')
        print(row, flush=True)
    invalid = [
        ('wrong-channels', dict(input_shape=[1, 4, 'h', 'w'], output_shape=[1, 4, 'oh', 'ow'])),
        ('wrong-batch', dict(input_shape=[2, 3, 'h', 'w'], output_shape=[2, 3, 'oh', 'ow'])),
        ('wrong-rank', dict(input_shape=[3, 'h', 'w'], output_shape=[3, 'oh', 'ow'])),
        ('partial-dynamic', dict(input_shape=[1, 3, 'h', 960], output_shape=[1, 3, 'oh', 1920])),
        ('wrong-fixed-scale', dict(output_shape=[1, 3, 540, 960], scale=1)),
        ('wrong-dynamic-scale', dict(dynamic=True, scale=1)),
    ]
    rejected = []
    for name, arguments in invalid:
        path = out / (name + '.onnx')
        make_model(path, **arguments)
        with (out / (name + '.log')).open('x', encoding='utf-8') as log:
            result = subprocess.run([str(exe), str(path)], cwd=out, env=env,
                stdout=log, stderr=subprocess.STDOUT, timeout=30)
        # Contract and checked WinML failures are expected; a process crash is not.
        assert result.returncode in (1, 3), (name, result.returncode)
        rejected.append(dict(case=name, returncode=result.returncode, rejected=True))
        (out / 'rejections.json').write_text(json.dumps(rejected, indent=2), encoding='utf-8')
        print(rejected[-1], flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
