"""GPU pixel-contract checks for external ONNX outputs; generated weights never ship.

python test_output_modes.py <smoke.exe> <new-output-dir> <ffmpeg.exe>
Requires numpy, Pillow and onnx. Tests processing correctness, not trained quality.
"""
from pathlib import Path
import json
import os
import subprocess
import sys
sys.dont_write_bytecode = True
import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper
from PIL import Image
from test_native_pipeline import fixture


def model(path, scale=2, fixed=None, half=False, output_half=False, gain=1.0,
          legacy=False, metadata=None, bad=None):
    src, dst = ('rgb_lr', 'rgb_residual_x2') if legacy else ('input.1', 'restored.rgb')
    shape = [1, 3, fixed[1], fixed[0]] if fixed else ['batch', 3, 'height', 'width']
    out_shape = [1, 3, fixed[1]*scale, fixed[0]*scale] if fixed else ['batch', 3, 'oh', 'ow']
    scale_x = scale + 1 if bad == 'nonuniform' else scale
    if bad == 'partial': shape = [1, 3, 'height', 48]
    if bad == 'nhwc': shape, out_shape = [1, 'h', 'w', 3], [1, 'oh', 'ow', 3]
    if bad == 'batch': shape[0] = out_shape[0] = 2
    dtype = TensorProto.FLOAT16 if half else TensorProto.FLOAT
    odtype = TensorProto.FLOAT16 if output_half else TensorProto.FLOAT
    if bad == 'uint8': dtype = TensorProto.UINT8
    inits = [numpy_helper.from_array(np.array([1,1,scale,scale_x],np.float32),'scales'),
             numpy_helper.from_array(np.array([],np.float32),'roi'),
             numpy_helper.from_array(np.array(gain,np.float32),'gain')]
    nodes = [helper.make_node('Cast',[src],['float'],to=TensorProto.FLOAT),
             helper.make_node('Mul',['float','gain'],['rgb']),
             helper.make_node('Resize',['rgb','roi','scales'],['upscaled'],mode='nearest',
                              coordinate_transformation_mode='asymmetric',nearest_mode='floor'),
             helper.make_node('Cast',['upscaled'],[dst],to=odtype)]
    ins=[helper.make_tensor_value_info(src,dtype,shape)]
    outs=[helper.make_tensor_value_info(dst,odtype,out_shape)]
    if bad=='multi-input':
        ins.append(helper.make_tensor_value_info('extra',dtype,shape))
    if bad=='multi-output':
        nodes.append(helper.make_node('Identity',[dst],['extra']))
        outs.append(helper.make_tensor_value_info('extra',odtype,out_shape))
    graph=helper.make_graph(nodes,'output-contract',ins,outs,inits)
    value=helper.make_model(graph,opset_imports=[helper.make_opsetid('',11)],ir_version=7)
    if metadata: helper.set_model_props(value,{'tenriff.output_mode':metadata})
    onnx.checker.check_model(value)
    onnx.save(value,path)


def reference(rgba, scale, residual, gain, half=False, output_half=False):
    h,w=rgba.shape[:2]
    rgb=rgba[:,:,:3].astype(np.float32)/255
    if half: rgb=rgb.astype(np.float16).astype(np.float32)
    rgb=rgb*np.float32(gain)
    if output_half: rgb=rgb.astype(np.float16).astype(np.float32)
    detail=np.repeat(np.repeat(rgb,scale,0),scale,1)
    x=(np.arange(w*scale)+.5)/scale-.5; y=(np.arange(h*scale)+.5)/scale-.5
    x0=np.floor(x).astype(int);y0=np.floor(y).astype(int)
    fx=(x-x0)[None,:,None];fy=(y-y0)[:,None,None]
    xa=np.clip(x0,0,w-1);xb=np.clip(x0+1,0,w-1);ya=np.clip(y0,0,h-1);yb=np.clip(y0+1,0,h-1)
    source=rgba.astype(np.float64)/255
    base=(source[ya[:,None],xa]*(1-fx)+source[ya[:,None],xb]*fx)*(1-fy)+(source[yb[:,None],xa]*(1-fx)+source[yb[:,None],xb]*fx)*fy
    result=np.concatenate((np.clip(detail+(base[:,:,:3] if residual else 0),0,1)*base[:,:,3:4],base[:,:,3:4]),axis=2)
    return np.floor(result[:,:,[2,1,0,3]]*255+.5).astype(np.uint8)


def main():
    exe,out,ffmpeg=map(lambda a:Path(a).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=False)
    env={k.upper():v for k,v in os.environ.items()}
    # Nonuniform colors and alpha expose RGB/BGR mistakes and double base addition.
    rgba=fixture(73,59);Image.fromarray(rgba).save(out/'input.png')
    filtered=subprocess.run([str(ffmpeg),'-v','error','-threads','1','-filter_threads','1','-f','rawvideo','-pixel_format','rgba','-video_size','73x59','-i','pipe:0','-vf','format=gbrp,deblock=filter=weak:block=8:planes=7,format=rgba','-frames:v','1','-threads','1','-f','rawvideo','pipe:1'],input=rgba.tobytes(),capture_output=True,check=True,timeout=20)
    deblocked=np.frombuffer(filtered.stdout,dtype=np.uint8).reshape(59,73,4).copy();deblocked[:,:,3]=rgba[:,:,3]
    cases=[('image-x1',dict(scale=1)),('image-x2',{}),('image-x3',dict(scale=3)),
           ('image-x4-half',dict(scale=4,half=True,output_half=True)),
           ('image-x2-mixed',dict(output_half=True)),('image-x2-half-input',dict(half=True)),
           ('image-fixed-seams',dict(scale=4,fixed=(48,32))),
           ('residual-explicit',dict(gain=.125,mode='residual',residual=True)),
           ('residual-fixed-x3',dict(scale=3,fixed=(48,32),gain=.125,mode='residual',residual=True)),
           ('residual-metadata',dict(gain=.125,metadata='residual',residual=True)),
           ('legacy-auto',dict(gain=.125,legacy=True,residual=True)),
           ('override-legacy-image',dict(legacy=True,mode='image')),
           ('override-metadata-image',dict(metadata='residual',mode='image')),
           ('image-clamp',dict(gain=2))]
    reports=[]
    for name,args in cases:
        args=args.copy();mode=args.pop('mode','auto');residual=args.pop('residual',False)
        path=out/(name+'.onnx');model(path,**args)
        prefix=out/name
        with (out/(name+'.log')).open('x',encoding='utf-8') as log:
            run=subprocess.run([str(exe),str(path),str(out/'input.png'),str(prefix),mode],env=env,cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=60)
        assert run.returncode==0,(name,run.returncode)
        scale=args.get('scale',2)
        expected=reference(deblocked,scale,residual,args.get('gain',1),args.get('half',False),args.get('output_half',False))
        actual=np.fromfile(prefix.with_suffix('.bgra'),np.uint8).reshape(expected.shape)
        assert json.loads(prefix.with_suffix('.json').read_text())=={'width':73*scale,'height':59*scale}
        error=int(np.abs(actual.astype(np.int16)-expected.astype(np.int16)).max())
        assert error<=1,(name,error)
        reports.append(dict(case=name,max_byte_error=error,channels=int(actual.size),passed=True))
        (out/'summary.json').write_text(json.dumps(reports,indent=2))
        print(reports[-1],flush=True)
    for bad in ['nonuniform','partial','nhwc','batch','uint8','multi-input','multi-output','scale5','nonfinite']:
        name='reject-'+bad;path=out/(name+'.onnx')
        model(path,bad=bad,scale=5 if bad=='scale5' else 2,gain=float('nan') if bad=='nonfinite' else 1)
        with (out/(name+'.log')).open('x',encoding='utf-8') as log:
            run=subprocess.run([str(exe),str(path),str(out/'input.png'),str(out/name)],env=env,cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=45)
        assert run.returncode==7,(bad,run.returncode)
        assert not (out/(name+'.bgra')).exists()
        reports.append(dict(case=name,native_fallback=True,passed=True))
        (out/'summary.json').write_text(json.dumps(reports,indent=2))
        print(reports[-1],flush=True)


if __name__=='__main__':main()
