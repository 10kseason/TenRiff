'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const C = require('./core.js');
const Z = require('./zip.js');
const catalog = require('./catalog.js');
const I = require('./i18n.js');
const root = path.resolve(__dirname, '../..');
const actualSchema = JSON.parse(fs.readFileSync(path.join(root, 'docs/tenriff-skin.schema.json'), 'utf8').replace(/^\uFEFF/,''));
const basic = () => ({format:'tenriff-skin', version:1, name:'테스트 / テスト'});
const errors = value => C.validate(value, actualSchema).filter(issue => issue.severity === 'error');

test('offline and bundled schemas equal the source of truth', () => {
  assert.deepEqual(catalog.schema, actualSchema);
  const bundledSchema = JSON.parse(fs.readFileSync(path.join(root, 'skins/Tengear/tenriff-skin.schema.json'), 'utf8').replace(/^\uFEFF/, ''));
  assert.deepEqual(bundledSchema, actualSchema);
});
test('every client note shape survives common and per-mode editor round trips',()=>{
  for(const shape of ['rect','circle','triangle','pentagon','hexagon','square','diamond','arrow','hex']){
    const doc={...basic(),gameplay:{note_shape:shape,modes:{'4k':{note_shape:shape}}}};
    const reloaded=C.parse(C.serialize(doc));
    assert.deepEqual(errors(reloaded),[]);assert.equal(C.gameplay(reloaded,'4k').note_shape,shape);
  }
});
test('polygon previews retain lane width and black head and tail outlines with opt-out',()=>{
  const G=require('./gameplay.js');
  for(const shape of ['triangle','pentagon','hexagon','diamond','arrow','hex']){
    const points=G.noteShapeVertices(shape),xs=points.map(p=>p[0]);
    assert.ok(Math.abs(Math.min(...xs)+.5)<1e-9);assert.ok(Math.abs(Math.max(...xs)-.5)<1e-9);
    for(const height of [8,32,128])for(const enabled of [false,true]){
      const strokes=[],ctx=new Proxy({stroke(){strokes.push([this.strokeStyle,this.lineWidth,this.globalAlpha]);}},
        {get(target,key){return key in target?target[key]:()=>{};}});
      G.drawNoteShape(ctx,[0,100,72,100+height],'#FFFFFF',{note_shape:shape,note_border:enabled,note_outline_opacity:.6},.8);
      assert.deepEqual(strokes,enabled?[['#000000',1.5,.48]]:[]);
    }
  }
  assert.deepEqual(G.noteShapeVertices('hex'),G.noteShapeVertices('hexagon'));
});
test('note height accepts 50–400 percent for common and per-mode fields without changing ratios', () => {
  for (const ratio of [.5, 1, 4]) {
    const document = {...basic(), gameplay: {note_height_ratio: ratio, modes: {}}};
    for (let keys = 4; keys <= 16; ++keys) document.gameplay.modes[`${keys}k`] = {note_height_ratio: ratio};
    const reloaded = C.parse(C.serialize(document));
    assert.deepEqual(errors(reloaded), []);
    assert.equal(reloaded.gameplay.note_height_ratio, ratio);
    for (let keys = 4; keys <= 16; ++keys) assert.equal(C.gameplay(reloaded, `${keys}k`).note_height_ratio, ratio);
  }
  for (const [ratio, code] of [[.49, 'minimum'], [4.01, 'maximum']]) {
    const document = {...basic(), gameplay: {note_height_ratio: ratio, modes: {'16k': {note_height_ratio: ratio}}}};
    const found = errors(document);
    for (const path of ['gameplay.note_height_ratio', 'gameplay.modes.16k.note_height_ratio'])
      assert.ok(found.some(issue => issue.path === path && issue.code === code));
  }
  assert.deepEqual(errors({...basic(), gameplay: {note_width_ratio: .1}}), [], 'width keeps its independent bound');
});
test('bundled/example manifests validate and survive round trips, including unknown fields', () => {
  const manifests = [path.join(root, 'examples/skins/TenRiff-Example/skin.json')];
  for (const entry of fs.readdirSync(path.join(root,'skins'), {withFileTypes:true})) {
    const manifest = path.join(root,'skins',entry.name,'skin.json');
    if(entry.isDirectory() && fs.existsSync(manifest)) manifests.push(manifest);
  }
  assert.ok(manifests.length >= 10);
  for (const filename of manifests) {
    const original = C.parse(fs.readFileSync(filename,'utf8'));
    assert.deepEqual(errors(original), [], filename);
    const extended = {...original, future_feature: {kept:['한글','日本語',42]}};
    const updated = C.set(extended,['theme','accent'],'#1256AB88');
    assert.deepEqual(C.parse(C.serialize(updated)).future_feature, extended.future_feature);
    assert.deepEqual(original, C.parse(C.serialize(original)), filename);
    assert.ok(C.validate(extended,actualSchema).some(issue=>issue.code==='unknown'&&issue.severity==='warning'));
  }
});
test('edits preserve sibling fields, deletion omits only the requested key', () => {
  const original = {...basic(), theme:{text:'#ffffff',accent:'#aaffbb'},future:{nested:7}};
  const next = C.set(original,['theme','accent'],undefined);
  assert.deepEqual(next.theme,{text:'#ffffff'}); assert.equal(original.theme.accent,'#aaffbb');assert.deepEqual(next.future,original.future);
});
test('JSON prototype-shaped properties remain inert and survive edits', () => {
  const original=C.parse('{"format":"tenriff-skin","version":1,"name":"safe","__proto__":{"polluted":true},"constructor":{"value":1}}');
  const changed=C.set(original,['__proto__','more'],3);
  assert.equal({}.polluted,undefined);assert.equal(changed.__proto__.polluted,true);assert.equal(changed.__proto__.more,3);
  assert.equal(Object.getPrototypeOf(changed),Object.prototype);
  assert.equal(C.get(changed,['toString']),undefined);
});
test('history is bounded, branching clears redo, save markers track real content', () => {
  const h=C.createHistory(basic(),2);h.commit({...basic(),name:'one'});h.markSaved();h.commit({...basic(),name:'two'});h.commit({...basic(),name:'three'});
  assert.equal(h.dirty,true);h.undo();h.undo();assert.equal(h.document.name,'one');assert.equal(h.dirty,false);assert.equal(h.undo(),false);
  h.redo();h.commit({...basic(),name:'branch'});assert.equal(h.canRedo,false);h.load(basic());assert.equal(h.dirty,false);assert.equal(h.canUndo,false);
});
test('an asynchronous export saves its snapshot without hiding newer edits', () => {
  const h=C.createHistory(basic());h.commit({...basic(),name:'Export snapshot'});const exported=h.document;
  h.commit({...basic(),name:'Newer edit'});h.markSaved(exported);assert.equal(h.dirty,true);
  h.undo();assert.equal(h.dirty,false);
});
test('invalid format, types, ranges and rectangle ordering are errors', () => {
  const value={format:'other',version:2,name:'',theme:{accent:'blue'},gameplay:{note_width_ratio:5,show_hold_tail:'true'},layout:{title:{buttons:[100,100,90,90]}}};
  const codes=errors(value).map(issue=>issue.code);for(const code of ['constant','minLength','pattern','maximum','type','rectangle'])assert.ok(codes.includes(code),code);
  assert.throws(()=>C.parse('[]'),/rootObject/);assert.throws(()=>C.parse('{bad'));
});
test('asset paths reject URL, absolute paths, traversal and unsupported images', () => {
  for(const bad of ['../outside.png','a/../../outside.png','C:\\secret.png','/secret.png','https://example.com/image.png','data:image/png;base64,xx','x.svg','a\u0000.png']) {
    assert.equal(C.safeAsset(bad),false,bad);assert.ok(errors({...basic(),lobby:{background:bad}}).some(issue=>issue.code==='assetPath'),bad);
  }
  for(const safe of ['', 'lobby/배경.png','日本語/背景.JPG','gameplay/note-{index:02}.png','a\\b.bmp'])assert.equal(C.safeAsset(safe),true,safe);
  assert.equal(C.normalizeAsset('lobby/./background.png'),'lobby/background.png');
  assert.equal(C.normalizeAsset('lobby\\background.png'),'lobby/background.png');
});
test('asset arrays and lane templates validate and enumerate per-mode overrides', () => {
  const value={...basic(),lobby:{logo:'logo.png'},native:{assets:{mark:'native/mark.png'}},gameplay:{note:'notes/{index:02}.png',modes:{'4k':{note:'arrows/{lane}.png',lane_map:['left','down','up','right']},'7+1':{note:['scratch.png','note.png']}}}};
  assert.deepEqual(errors(value),[]);
  const refs=C.assetReferences(value);for(const expected of ['logo.png','native/mark.png','notes/16.png','arrows/left.png','arrows/right.png','scratch.png'])assert.ok(refs.includes(expected),expected);
  assert.ok(errors({...basic(),gameplay:{note:['../bad.png']}}).some(issue=>issue.code==='assetPath'));
});
test('native additive rectangles allow negative values; property names and font UTF-8 lengths are checked', () => {
  const value={...basic(),lobby:{renderer:'native'},native:{rects:{'title.rect.001':[-10,10,-20,5]},fonts:{title:'游ゴシック'}}};assert.deepEqual(errors(value),[]);
  const bad={...value,native:{metrics:{'bad/key':1,'valid.key':Infinity},motion:{speed:-1},fonts:{title:'가'.repeat(50)}}};
  const codes=errors(bad).map(issue=>issue.code);for(const expected of ['pattern','finite','minimum','fontBytes'])assert.ok(codes.includes(expected),expected);
});
test('every catalog slot uses a valid native schema key and default value', () => {
  const data=JSON.parse(fs.readFileSync(path.join(__dirname,'native-catalog.json'),'utf8').replace(/^\uFEFF/,''));
  const native=Object.fromEntries(['metrics','colors','rects','assets','motion','fonts'].map(key=>[key,data[key]||{}]));
  assert.deepEqual(errors({...basic(),lobby:{renderer:'native'},native}),[]);
  assert.ok(Object.keys(native.rects).length > 100);
});
test('move/resize keeps valid rectangles and rounds once per action', () => {
  assert.deepEqual(C.moveRect([10,20,110,120],13,19,false,8),[26,36,126,136]);
  assert.deepEqual(C.moveRect([10,20,110,120],-500,-500,true,1),[10,20,18,28]);
  assert.deepEqual(C.moveRect([8100,8100,8190,8190],100,100,false,1),[8102,8102,8192,8192]);
});
test('native title canvas defaults match the native renderer slots', () => {
  assert.deepEqual(C.nativeTitleLayout,{logo:[56,24,418,94],buttons:[1016,260,1824,824],guide:[96,516,932,924],footer:[64,972,1856,1048],spectrum:[1430,990,1574,1028]});
  assert.deepEqual(errors({...basic(),lobby:{renderer:'native'},layout:{title:C.nativeTitleLayout}}),[]);
});
test('all UI language rows contain Korean, English and Japanese text', () => {
  for(const [key,row]of Object.entries(I.rows)){assert.equal(row.length,3,key);row.forEach(value=>assert.ok(typeof value==='string'&&value.length,key));}
  assert.equal(I.text('save','ja'),'名前を付けて保存');assert.equal(I.text('unknown.technical.key','ko'),'unknown.technical.key');
});
test('ZIP headers, UTF-8 paths and CRC preserve exact bytes', () => {
  const entries=[{name:'테스트/skin.json',data:new TextEncoder().encode(C.serialize(basic()))},{name:'테스트/背景.png',data:new Uint8Array([137,80,78,71,0,1,2,255])}];
  const zip=Buffer.concat(Z.create(entries).map(part=>Buffer.from(part)));let offset=0;
  for(const expected of entries){assert.equal(zip.readUInt32LE(offset),0x04034b50);assert.equal(zip.readUInt16LE(offset+6),0x800);const length=zip.readUInt32LE(offset+18),nameLength=zip.readUInt16LE(offset+26);assert.equal(zip.subarray(offset+30,offset+30+nameLength).toString('utf8'),expected.name);const data=zip.subarray(offset+30+nameLength,offset+30+nameLength+length);assert.deepEqual(data,Buffer.from(expected.data));assert.equal(zip.readUInt32LE(offset+14),Z.crc32(data));offset+=30+nameLength+length;}
  assert.equal(zip.readUInt32LE(offset),0x02014b50);assert.equal(zip.readUInt32LE(zip.length-22),0x06054b50);assert.equal(zip.readUInt16LE(zip.length-12),2);assert.equal(Z.crc32(new TextEncoder().encode('123456789')),0xcbf43926);
  assert.throws(()=>Z.create([{name:'../escape',data:new Uint8Array()}]),/Unsafe/);assert.throws(()=>Z.create([{name:'a.png',data:new Uint8Array()},{name:'A.PNG',data:new Uint8Array()}]),/duplicate/);
});
test('scripts parse without build tools and HTML declares an offline policy', () => {
  for(const name of ['core.js','gameplay.js','zip.js','i18n.js','catalog.js','native-catalog.js','app.js'])new vm.Script(fs.readFileSync(path.join(__dirname,name),'utf8'),{filename:name});
  const html=fs.readFileSync(path.join(__dirname,'index.html'),'utf8');assert.match(html,/connect-src 'none'/);assert.doesNotMatch(html,/<script[^>]+https?:/);assert.match(html,/webkitdirectory/);
});

test('native gameplay overrides merge by category and preserve unrelated mode fields', () => {
  const original={...basic(),gameplay:{renderer:'native',note:'common.png',native:{motion:{press_depth:7,release_response:24},colors:{combo:'#123456'},sprites:{note:[{x:0,y:0,width:128,height:32,color:'#FFFFFF',mix:0,alpha:1,radius:0}]}},modes:{'4k':{note:'four.png',native:{motion:{press_depth:3},sprites:{note:[]}}}}}};
  const before=C.serialize(original),four=C.gameplay(original,'4k'),sixteen=C.gameplay(original,'16k');
  assert.deepEqual(four.native.motion,{press_depth:3,release_response:24});
  assert.deepEqual(four.native.colors,{combo:'#123456'});assert.deepEqual(four.native.sprites.note,[]);
  assert.equal(four.renderer,'native');assert.equal(four.note,'four.png');assert.equal(sixteen.note,'common.png');
  assert.equal(sixteen.native.sprites.note.length,1);assert.equal(C.serialize(original),before);
});
test('all native gameplay controls and every 4K–16K palette come from client data',()=>{
  const G=require('./gameplay.js'),source=JSON.parse(fs.readFileSync(path.join(root,'assets/native-gameplay/luma-keys/editor-catalog.json'),'utf8'));
  for(const group of G.groups)assert.deepEqual(catalog.gameplayNative[group],source[group]);
  assert.equal(catalog.native.gameplay.renderer,'native');assert.deepEqual(errors(catalog.native),[]);
  for(let count=4;count<=16;count++)assert.equal(catalog.gameplayNative.palettes[count+'k'].length,count);
  const native=G.settings(catalog.native.gameplay,catalog.gameplayNative);
  assert.equal(native.motion.release_response,24);assert.equal(native.sprites.key_idle.length,source.sprites.key_idle.default.length);
  assert.deepEqual(C.assetReferences(catalog.native),[],'vector-only default exports without missing image files');
});
test('sprite and motion validation rejects invalid edits before download',()=>{
  const next=C.clone(catalog.native);next.gameplay.native.motion.release_response=0;
  next.gameplay.native.sprites.note=[{x:0,y:0,width:128,height:500,color:'#FFFFFF',mix:2,alpha:1,radius:0}];
  const found=errors(next);assert.ok(found.some(x=>x.path.endsWith('release_response')));assert.ok(found.some(x=>x.path.endsWith('mix')));assert.ok(found.some(x=>x.path.endsWith('height')));
  next.gameplay.native.motion.release_response=24;next.gameplay.native.sprites.note=[];assert.deepEqual(errors(next),[]);
});
test('editable sprites survive JSON and ZIP byte readback without game art dependencies',()=>{
  const doc=C.clone(catalog.native);doc.gameplay.native.sprites.key_idle[0].color='#AA33CC';
  doc.gameplay.modes={'16k':{native:{motion:{press_depth:12},sprites:{hold_tail:[]}}}};
  const bytes=new TextEncoder().encode(C.serialize(doc)),zip=Buffer.concat(Z.create([{name:'Luma-Edited/skin.json',data:bytes}]).map(Buffer.from));
  const start=30+zip.readUInt16LE(26),roundtrip=C.parse(zip.subarray(start,start+bytes.length).toString('utf8'));
  assert.deepEqual(roundtrip,doc);assert.deepEqual(errors(roundtrip),[]);assert.equal(Z.crc32(bytes),zip.readUInt32LE(14));
});
test('preview key response and explicit hidden art are stable across render rates',()=>{
  const G=require('./gameplay.js'),motion=G.settings(catalog.native.gameplay,catalog.gameplayNative).motion;
  for(const fps of [60,144,300,1000]){let value=1;const frames=Math.round(fps/2);for(let i=0;i<frames;i++)value=G.advance(value,false,.5/frames,motion);assert.ok(Math.abs(value-Math.exp(-12))<1e-10);}
  assert.deepEqual(G.settings({native:{sprites:{note:[]}}},catalog.gameplayNative).sprites.note,[]);
  assert.equal(G.blend('#112233','#AABBCC',0),'#112233');assert.equal(G.blend('#112233','#AABBCC',1),'#aabbcc');
});

test('key backdrop controls preserve explicit off and per-mode opacity through schema and JSON',()=>{
  const doc={...basic(),gameplay:{key_backdrop:false,key_backdrop_opacity:0,modes:{'4k':{key_backdrop:true,key_backdrop_opacity:.65}}}};
  assert.deepEqual(errors(doc),[]);assert.deepEqual(C.parse(C.serialize(doc)),doc);
  assert.equal(C.gameplay(doc,'4k').key_backdrop_opacity,.65);assert.equal(C.gameplay(doc,'10k').key_backdrop,false);
  assert.ok(errors({...basic(),gameplay:{key_backdrop_opacity:1.01}}).some(x=>x.code==='maximum'));
  assert.ok(errors({...basic(),gameplay:{key_backdrop:'false'}}).some(x=>x.code==='type'));
});
test('preview backdrop switches off independently and judgement line tracks note height',()=>{
  const G=require('./gameplay.js');
  assert.equal(G.backdropOpacity({key_backdrop:false,key_backdrop_opacity:1,key_pulse_brightness:1},true),0);
  assert.equal(G.backdropOpacity({key_backdrop:true,key_backdrop_opacity:.4,key_pulse_brightness:0},true),.4);
  assert.equal(G.backdropOpacity({key_backdrop_opacity:1},false),0);
  assert.equal(G.judgementLineWidth(2,1),2);
  assert.equal(G.judgementLineWidth(5,2),10);
  assert.ok(Math.abs(G.judgementLineWidth(2,4/1.8)/G.judgementLineWidth(2,.5/1.8)-8)<1e-10);
});

test('native combo and judgement previews outline text before fill and preserve alpha',()=>{
  const G=require('./gameplay.js');
  for(const role of ['combo','judgement'])for(const size of [8,42,52,144]){
    const calls=[],ctx={save(){},restore(){},beginPath(){},clip(){},
      rect(...args){calls.push(['clip',...args]);},
      strokeText(...args){calls.push(['stroke',this.lineWidth,this.strokeStyle,this.globalAlpha,...args]);},
      fillText(...args){calls.push(['fill',this.globalAlpha,...args]);}};
    G.paintText(ctx,'147',[100,200,260,258],role,size,'#FFFFFF',.4,'Bahnschrift');
    assert.deepEqual(calls.map(call=>call[0]),['clip','stroke','fill']);
    assert.ok(calls[1][1]>=1&&calls[1][1]<=1.5,'outline remains thin at every authored font size');
    assert.equal(calls[1][2],'#061118');assert.equal(calls[1][3],.4);assert.equal(calls[2][1],.4);
    assert.equal(calls[1][5],100);assert.equal(calls[1][6],229,'authored text center remains unchanged');
    assert.ok(calls[0][4]>=Math.max(58,size*1.5),'large glyphs fit the vertical clip');
    assert.equal(ctx.font,`600 ${size}px "Bahnschrift", sans-serif`);
  }
});

test('all preview labels outline their original fill with contrast for light and dark text',()=>{
  const G=require('./gameplay.js');
  for(const [color,outline]of [['#AABBCC','#061118'],['#0B1620','#EAF3FD']]){
    const calls=[],ctx={save(){},restore(){},beginPath(){},clip(){},
      rect(...args){calls.push(['clip',...args]);},strokeText(...args){calls.push(['stroke',this.strokeStyle,this.globalAlpha,...args]);},
      fillText(...args){calls.push(['fill',this.fillStyle,this.globalAlpha,...args]);}};
    G.paintText(ctx,'SCORE',[100,200,260,258],'score',30,color,.6);
    assert.deepEqual(calls[0],['clip',99,199,162,60]);
    assert.deepEqual(calls[1],['stroke',outline,.6,'SCORE',100,229,160]);
    assert.deepEqual(calls[2],['fill',color,.6,'SCORE',100,229,160]);
  }
});

test('backdrop brightness and height roundtrip with per-mode overrides and match runtime limits',()=>{
  const G=require('./gameplay.js');
  const doc={...basic(),gameplay:{key_backdrop_brightness:1.5,key_backdrop_height:.8,
    modes:{'4k':{key_backdrop_brightness:.5,key_backdrop_height:.25}}}};
  assert.deepEqual(errors(doc),[]);assert.deepEqual(C.parse(C.serialize(doc)),doc);
  assert.equal(C.gameplay(doc,'4k').key_backdrop_brightness,.5);
  assert.equal(C.gameplay(doc,'4k').key_backdrop_height,.25);
  assert.equal(C.gameplay(doc,'10k').key_backdrop_brightness,1.5);
  for(const value of [{key_backdrop_brightness:2.01},{key_backdrop_height:1.01}])
    assert.ok(errors({...basic(),gameplay:value}).some(x=>x.code==='maximum'));
  assert.equal(G.backdropColor('#4080C0',{}),'#4080C0');
  assert.equal(G.backdropColor('#4080C0',{key_backdrop_brightness:.5}),'#204060');
  assert.equal(G.backdropColor('#4080C0',{key_backdrop_brightness:2}),'#80FFFF');
  assert.equal(G.backdropColor('#4080C080',{key_backdrop_brightness:0}),'#00000080');
  assert.equal(G.backdropTop({},2,1078),2);
  assert.equal(G.backdropTop({key_backdrop_height:.5},2,1078),540);
  assert.equal(G.backdropTop({key_backdrop_height:0},2,1078),1078);
  assert.equal(G.backdropTop({key_backdrop_height:.25},50,1060),807.5);
  assert.equal(G.backdropOpacity({key_backdrop_brightness:2,key_backdrop_height:.25,key_backdrop_opacity:.4},true),.4);
});

test('native preview retains timing through the short judgement animation',()=>{
  const G=require('./gameplay.js');
  for(const [now,animate,enabled,visible] of [[0,false,true,false],[100,true,true,false],[3100,true,true,true],[6100,true,true,true],[3499,true,true,true],[3100,true,false,false]]){
    const bars=[],labels=[];
    const ctx=new Proxy({createLinearGradient(){return {addColorStop(){}};},fillRect(){if(this.fillStyle==='#123456')bars.push(true);},fillText(value){labels.push(value);}},
      {get(target,key){return key in target?target[key]:()=>{};}});
    const style=C.clone(catalog.native.gameplay);style.show_timing_feedback=enabled;
    style.native.colors.timing='#123456';
    G.paint(ctx,style,catalog.gameplayNative,'4k',now,animate,()=>false);
    assert.equal(bars.length>0,visible,JSON.stringify({now,animate,enabled}));
    assert.equal(labels.some(value=>/^(FAST|SLOW) /.test(value)),visible);
  }
});

test('FAST/SLOW text and bar can each be disabled independently',()=>{
  const G=require('./gameplay.js');
  for(const text of [false,true])for(const bar of [false,true]){
    const bars=[],labels=[];
    const ctx=new Proxy({createLinearGradient(){return {addColorStop(){}};},fillRect(){if(this.fillStyle==='#123456')bars.push(true);},fillText(value){labels.push(value);}},
      {get(target,key){return key in target?target[key]:()=>{};}});
    const style=C.clone(catalog.native.gameplay);
    style.show_timing_feedback=text;style.show_timing_bar=bar;style.native.colors.timing='#123456';
    G.paint(ctx,style,catalog.gameplayNative,'4k',3100,true,()=>false);
    assert.equal(bars.length>0,bar);
    assert.equal(labels.some(value=>/^(FAST|SLOW) /.test(value)),text);
    assert.deepEqual(errors({...basic(),gameplay:{show_timing_feedback:text,show_timing_bar:bar}}),[]);
  }
});

test('native and bitmap LN bodies use linear configured alpha',()=>{
  const G=require('./gameplay.js');
  for(const opacity of [1,.5,.45,0]) {
    assert.deepEqual(errors({...basic(),gameplay:{hold_body_opacity:opacity}}),[]);
    const nativeAlphas=[],bitmapAlphas=[];
    const gradient={addColorStop(){}};
    const ctx=new Proxy({globalAlpha:1,createLinearGradient(){return gradient;},fillRect(){if(this.fillStyle===gradient)nativeAlphas.push(this.globalAlpha);}},
      {get(target,key){return key in target?target[key]:()=>{};}});
    const style=C.clone(catalog.native.gameplay);style.visual_opacity=1;style.hold_body_opacity=opacity;
    G.paint(ctx,style,catalog.gameplayNative,'4k',0,false,()=>false);
    assert.ok(nativeAlphas.length>0);
    nativeAlphas.forEach(value=>assert.equal(value,opacity));
    style.hold_body='test-body.png';
    G.paint(ctx,style,catalog.gameplayNative,'4k',0,false,(file,rect,alpha)=>{
      if(file==='test-body.png')bitmapAlphas.push(alpha);return true;
    });
    assert.ok(bitmapAlphas.length>0);
    bitmapAlphas.forEach(value=>assert.equal(value,opacity));
  }
});

test('always timing bar remains visible on PG and text survives only 750ms after the last non PG',()=>{
  const G=require('./gameplay.js');
  for(const [now,always,bar,text] of [[0,true,true,false],[0,false,false,false],[15100,false,true,true],[15400,false,false,false],[15400,true,true,false]]){
    const bars=[],labels=[];
    const ctx=new Proxy({createLinearGradient(){return {addColorStop(){}};},fillRect(){if(this.fillStyle==='#123456')bars.push(true);},fillText(value){labels.push(value);}},
      {get(target,key){return key in target?target[key]:()=>{};}});
    const style=C.clone(catalog.native.gameplay);
    style.show_timing_feedback=true;style.show_timing_bar=true;style.timing_bar_always_visible=always;style.native.colors.timing='#123456';
    G.paint(ctx,style,catalog.gameplayNative,'4k',now,true,()=>false);
    assert.equal(bars.length>0,bar,JSON.stringify({now,always}));
    assert.equal(labels.some(value=>/^(FAST|SLOW) /.test(value)),text,JSON.stringify({now,always}));
  }
  const doc={...basic(),gameplay:{timing_bar_always_visible:true,modes:{'4k':{timing_bar_always_visible:false}}}};
  assert.deepEqual(errors(doc),[]);
  assert.deepEqual(C.parse(C.serialize(doc)),doc);
  assert.equal(C.gameplay(doc,'4k').timing_bar_always_visible,false);
  assert.equal(C.gameplay(doc,'10k').timing_bar_always_visible,true);
});
