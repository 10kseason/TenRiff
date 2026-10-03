/* Offline authoring UI. No fetch, service worker, analytics, or automatic disk writes. */
(function () {
  'use strict';
  const C = window.TenRiffSkin, catalog = window.TenRiffCatalog, I = window.TenRiffI18n;
  const schema = catalog.schema, nativeCatalog = window.TENRIFF_NATIVE_CATALOG || {};
  const G = window.TenRiffGameplay, gameplayCatalog = catalog.gameplayNative || {};
  const $ = id => document.getElementById(id);
  const tabs = ['metadata', 'layout', 'theme', 'lobby', 'native', 'gameplay', 'instrument', 'json', 'help'];
  const screens = ['title', 'song_select', 'result', 'generic', ...schema.properties.lobby.properties.screen_backgrounds.propertyNames.enum.filter(x => !['title', 'song_select', 'result'].includes(x))];
  const modes = Array.from({length: 16}, (_, i) => `${i + 1}k`).concat('7+1');
  let language = /^ja/i.test(navigator.language) ? 'ja' : /^ko/i.test(navigator.language) ? 'ko' : 'en';
  try { language = localStorage.getItem('tenriff-skin-editor-language') || language; } catch (_) { /* file:// storage can be disabled. */ }
  if (!I.languages.includes(language)) language = 'en';
  let tab = 'layout', screen = 'title', mode = 'common', previewMode = '7k', slot = 'buttons';
  let nativeGroup = 'metrics', nativeSearch = '', rawText = '', rawDirty = false, drag = null, imageTarget = null;
  let instrumentGroup = 'motion', spriteSlot = 'key_idle', spriteLayer = 0, pressedPreviewLane = -1;
  let raf = 0, busy = false, documentGeneration = 0;
  let selectedOption = 4;
  const files = new Map(), images = new Map();
  const fallbackNative = {format: 'tenriff-skin', version: 1, name: 'Native Skin', lobby: {renderer: 'native'}, native: {metrics: {}, colors: {}, rects: {}, assets: {}, motion: {}, fonts: {}}};
  const history = C.createHistory(catalog.native || fallbackNative);
  const defaults = {
    theme: {accent:'#6EE7F2',text:'#F4F7FF',muted:'#9AA3AD',card:'#1F2130E8',panel:'#14141CB8',footer:'#0B101DF2',button:'#242638',button_selected:'#6EE7F238',border:'#31344A',judgement_line:'#FF4D6D',lane_divider:'#F6F8FF',scene_primary:'#61D6FA',scene_secondary:'#8F9EFA',scene_background:'#04081A'},
    gameplay: {background_opacity:.66,note_width_ratio:1,note_height_ratio:1,judgement_line_position:.82,full_lane_receptors:false,note_aspect:'stretch',show_lane_dividers:true,show_judgement_line:true,show_timing_feedback:true,show_gear_boundary_line:true,show_hold_tail:true,hold_tail_taper:true,judgement_line_glow:true,key_pulse:true,key_pulse_brightness:.7,key_backdrop:true,key_backdrop_opacity:.25,key_backdrop_brightness:1,key_backdrop_height:1,hit_burst_style:'prism',key_label_position:'bottom',note_border:true,note_shape:'rect',lane_background_opacity:.08,black_playfield:true,visual_opacity:1,note_outline_opacity:1,hold_body_opacity:.28}
  };
  const layouts = {
    title: {spectrum:[696,66,1224,150],logo:[670,184,1250,300],buttons:[470,360,1450,940],guide:[1492,386,1834,924],footer:[80,972,1840,1056]},
    song_select: {top_bar:[0,0,1920,126],logo:[48,18,388,108],nav:[492,12,1244,126],profile:[1518,16,1880,112],avatar:[1530,26,1606,102],left_panel:[38,152,486,922],center_panel:[510,152,1266,922],right_panel:[1290,152,1882,922],bottom_bar:[38,944,1882,1048]},
    generic: {content:[64,154,1856,958],preview:[1120,180,1780,940]},
    result: {profile:[1400,28,1848,108],avatar:[1415,37,1477,99],song_panel:[70,154,590,738],analysis_panel:[1320,154,1848,738],stats_panel:[70,770,1270,994],continue:[1320,770,1848,856],replay:[1320,872,1574,942],retry:[1594,872,1848,942]}
  };
  const t = key => I.text(key, language);
  const doc = () => history.document;
  function element(tag, props = {}, parent) {
    const node = document.createElement(tag);
    for (const [key, value] of Object.entries(props)) {
      if (key === 'text') node.textContent = value;
      else if (key === 'class') node.className = value;
      else if (key === 'on') Object.entries(value).forEach(([event, callback]) => node.addEventListener(event, callback));
      else if (key in node) node[key] = value;
      else node.setAttribute(key, value);
    }
    if (parent) parent.append(node);
    return node;
  }
  function option(select, value, text) { element('option', {value, text}, select); }
  function notice(message, kind = '') { $('notice').textContent = message; $('notice').className = `notice ${kind}`; $('notice').hidden = !message; }
  function canChange() { if (rawDirty) { notice(t('pendingRaw'), 'warning'); return false; } return true; }
  function issues(value = doc()) { return C.validate(value, schema); }
  function change(path, value) {
    if (!canChange()) return;
    history.commit(C.set(doc(), path, value));
    render();
  }
  function defaultValue(path, definition) {
    if (path[0] === 'native') return C.get(nativeCatalog, path.slice(1));
    if (path[0] === 'theme') return defaults.theme[path[1]];
    if (path[0] === 'gameplay') {
      const key = path[path.length - 1];
      if (path.length > 2 && C.get(doc(), ['gameplay', key]) !== undefined) return C.get(doc(), ['gameplay', key]);
      if (C.own(defaults.gameplay, key)) return defaults.gameplay[key];
    }
    if (definition.default !== undefined) return definition.default;
    if (definition.type === 'number') return definition.minimum || 0;
    if (definition.type === 'boolean') return false;
    if (definition.enum) return definition.enum[0];
    if (definition.type === 'array') return [];
    return '';
  }
  let fieldId = 0;
  function field(parent, path, sourceDefinition, settings = {}) {
    const definition = C.resolve(sourceDefinition, schema), value = C.get(doc(), path), inherited = value === undefined;
    const fallback = settings.fallback !== undefined ? settings.fallback : defaultValue(path, definition);
    const shown = inherited ? fallback : value, key = path[path.length - 1], id = `field-${++fieldId}`;
    const wrap = element('div', {class: `field${inherited ? '' : ' active'}`}, parent);
    const heading = element('div', {class:'field-heading'}, wrap);
    const label = element('label', {htmlFor:id, text:settings.label || t(key)}, heading);
    element('code', {text:path.join('.')}, label);
    if (!settings.required) element('button', {type:'button', text:t('reset'), disabled:inherited, on:{click:() => change(path, undefined)}}, heading);
    const controls = element('div', {class:'field-controls'}, wrap);
    const update = newValue => change(path, newValue);
    const color = sourceDefinition.$ref === '#/$defs/hexColor';
    const asset = sourceDefinition.$ref === '#/$defs/assetPath' || sourceDefinition.$ref === '#/$defs/laneAssets';
    const rectangle = sourceDefinition.$ref === '#/$defs/nativeRectAdjustment';
    if (rectangle) {
      controls.className = 'rect-fields';
      ['dx','dy','dw','dh'].forEach((part,index) => {
        const partLabel = element('label', {text:t(part)}, controls);
        element('input', {id:index===0?id:`${id}-${index}`, type:'number',min:-8192,max:8192,step:1,value:Array.isArray(shown)?shown[index]:0,on:{change:event=>{
          const values = Array.isArray(shown) ? [...shown] : [0,0,0,0];
          values[index] = event.target.valueAsNumber;
          if (Number.isFinite(values[index])) update(values);
        }}}, partLabel);
      });
    } else if (definition.enum || definition.type === 'boolean') {
      const select = element('select', {id,on:{change:event=>update(definition.type === 'boolean' ? event.target.value === 'true' : event.target.value)}}, controls);
      const values = definition.enum || [true,false];
      values.forEach(item => option(select, String(item), typeof item === 'boolean' ? t(item?'enabled':'disabled') : t(path[0]==='gameplay'&&key==='renderer'&&item==='native'?'rendererNative':item)));
      if (!values.includes(shown) && shown !== undefined) option(select, String(shown), String(shown));
      select.value = String(shown);
    } else if (definition.type === 'number' || definition.type === 'integer') {
      element('input', {id,type:'number',step:definition.type==='integer'?1:'any',min:definition.minimum??'',max:definition.maximum??'',value:shown??'',on:{change:event=>{
        if (Number.isFinite(event.target.valueAsNumber)) update(event.target.valueAsNumber);
      }}}, controls);
    } else if (definition.type === 'array' || (definition.oneOf && Array.isArray(shown))) {
      element('textarea', {id,value:JSON.stringify(shown??[],null,2),rows:3,spellcheck:false,on:{change:event=>{
        try { const parsed=JSON.parse(event.target.value); if(!Array.isArray(parsed)) throw new Error(t('type')+' array'); update(parsed); }
        catch(error) { notice(error.message,'error'); event.target.setAttribute('aria-invalid','true'); }
      }}}, controls);
    } else {
      if (color) {
        const normalized = String(shown || '#ffffff').replace(/^#/,'');
        element('input',{type:'color',value:`#${normalized.slice(0,6).padEnd(6,'0')}`,'aria-label':t(key),on:{change:event=>{
          update(event.target.value.toUpperCase() + (normalized.length === 8 ? normalized.slice(6) : ''));
        }}},controls);
      }
      const input = element('input', {id,type:'text',value:shown??'',spellcheck:false,on:{change:event=>{
        if(definition.oneOf&&event.target.value.trim().startsWith('[')){
          try{const parsed=JSON.parse(event.target.value);if(!Array.isArray(parsed))throw new Error(t('type')+' array');update(parsed);}catch(error){notice(error.message,'error');}
        }else update(event.target.value);
      }}}, controls);
      if (asset) input.placeholder = 'folder/image.png';
      if (asset) element('button', {class:'asset-button',text:t('choose'),type:'button',on:{click:()=>{
        if(!canChange())return; imageTarget=path; $('image-input').multiple=false; $('image-input').click();
      }}},controls);
    }
    element('span',{class:'field-status',text:inherited?t('inherited'):t('overridden')},wrap);
    if (asset) element('p',{class:'hint',text:t('pathHint')},wrap);
    return wrap;
  }
  function renderInspector() {
    const body = $('inspector-body'); body.replaceChildren(); fieldId=0;
    $('inspector-title').textContent=t(tab);
    if (tab === 'metadata') {
      field(body,['name'],schema.properties.name,{required:true}); field(body,['author'],schema.properties.author);
      element('p',{class:'hint',text:t('optionalHint')},body);
      element('label',{text:t('newTemplate'),htmlFor:'new-template'},body);
      const select=element('select',{id:'new-template',class:'wide'},body); option(select,'native',t('defaultSkin'));option(select,'example',t('sampleSkin'));
      element('button',{text:t('new'),on:{click:()=>newSkin(select.value)}},body);
    } else if (tab === 'theme') {
      element('p',{class:'hint',text:t('optionalHint')},body);
      Object.entries(schema.properties.theme.properties).forEach(([key,rule])=>field(body,['theme',key],rule));
    } else if (tab === 'lobby') {
      const props=schema.properties.lobby.properties;
      for(const [key,rule] of Object.entries(props)) if(!['screen_backgrounds','screen_opacities'].includes(key)) field(body,['lobby',key],rule,{fallback:key==='background_opacity'?.72:undefined});
      element('h3',{text:t('screen_backgrounds')},body);
      element('p',{class:'hint',text:t(screen)},body);
      if(screen!=='generic') {
        field(body,['lobby','screen_backgrounds',screen],props.screen_backgrounds.additionalProperties,{label:t('background')});
        field(body,['lobby','screen_opacities',screen],props.screen_opacities.additionalProperties,{label:t('background_opacity'),fallback:C.get(doc(),['lobby','background_opacity'])??.72});
      }
    } else if(tab === 'gameplay') {
      element('p',{class:'hint',text:t('gameplayHint')},body);
      const select=element('select',{class:'wide','aria-label':t('mode'),on:{change:e=>{if(!canChange()){e.target.value=mode;return;}mode=e.target.value;if(mode!=='common'){previewMode=mode;$('preview-mode').value=mode;}renderInspector();draw();}}},body);
      option(select,'common',t('common'));modes.forEach(m=>option(select,m,m.toUpperCase()));select.value=mode;
      const base=mode==='common'?['gameplay']:['gameplay','modes',mode];
      const props=mode==='common'?schema.properties.gameplay.properties:schema.$defs.gameplayMode.properties;
      Object.entries(props).filter(([key])=>!['modes','native'].includes(key)).forEach(([key,rule])=>field(body,[...base,key],rule));
    } else if(tab === 'instrument') {
      renderInstrument(body);
    } else if(tab === 'layout') {
      element('p',{class:'hint',text:t('absoluteHint')},body);
      const slots=Object.keys(layouts[screen]||layouts.generic);if(!slots.includes(slot))slot=slots[0];
      element('label',{text:t('selectSlot'),htmlFor:'slot-select'},body);
      const select=element('select',{id:'slot-select',class:'wide',on:{change:e=>{slot=e.target.value;renderInspector();draw();}}},body);
      slots.forEach(key=>option(select,key,t(key)));select.value=slot;
      const rect=effectiveRect(screen,slot), path=['layout',screen,slot], value=C.get(doc(),path);
      const container=element('div',{class:'rect-fields'},body);
      ['left','top','right','bottom'].forEach((key,index)=>{
        const label=element('label',{text:t(key)},container);
        element('input',{type:'number',min:-8192,max:8192,step:1,value:rect[index],on:{change:e=>{
          const next=[...rect];next[index]=e.target.valueAsNumber;if(Number.isFinite(next[index]))change(path,next);
        }}},label);
      });
      element('div',{class:'field-status',text:value===undefined?t('inherited'):t('overridden')},body);
      element('button',{text:t('reset'),disabled:value===undefined,on:{click:()=>change(path,undefined)}},element('div',{class:'action-row'},body));
      element('p',{class:'hint',text:t('helpKeys')},body);
    } else if(tab==='native') {
      element('p',{class:'hint',text:t('nativeHint')},body);
      element('p',{class:'hint',text:t('runtimeBounds')},body);
      field(body,['lobby','renderer'],schema.properties.lobby.properties.renderer||{enum:['legacy','native']});
      const select=element('select',{class:'wide','aria-label':t('group'),on:{change:e=>{nativeGroup=e.target.value;renderInspector();}}},body);
      ['metrics','colors','rects','assets','motion','fonts'].forEach(group=>option(select,group,`${t(group)} (${Object.keys({...nativeCatalog[group],...C.get(doc(),['native',group])}).length})`));select.value=nativeGroup;
      if(nativeGroup==='rects')element('p',{class:'hint',text:t('deltaHint')},body);
      const search=element('input',{class:'wide',type:'search',placeholder:t('search'),value:nativeSearch,'aria-label':t('search')},body);
      const list=element('div',{class:'native-list'},body);
      const renderList=()=>{
        list.replaceChildren();const keys=Object.keys({...nativeCatalog[nativeGroup],...C.get(doc(),['native',nativeGroup])}).sort();
        const filtered=keys.filter(key=>key.toLowerCase().includes(nativeSearch.toLowerCase()));
        if(!filtered.length)element('p',{class:'hint empty',text:t(Object.keys(nativeCatalog).length?'noMatches':'nativeMissing')},list);
        const rule=schema.properties.native?.properties[nativeGroup]?.additionalProperties||{type:'string'};
        filtered.forEach(key=>{
          const localized=C.get(nativeCatalog,['labels',`${nativeGroup}.${key}`,language]);
          const range=nativeCatalog.ranges?.[`${nativeGroup}.${key}`]||
            (nativeGroup==='motion'?({enabled:{min:0,max:1},speed:{min:0,max:4},intensity:{min:0,max:2},entry_seconds:{min:.05,max:2}}[key]):
              nativeGroup==='metrics'&&/^font\..*\.size$/.test(key)?{min:8,max:180}:null);
          const bounded=range?{...rule,minimum:range.min,maximum:range.max}:rule;
          field(list,['native',nativeGroup,key],bounded,{label:localized||key,fallback:nativeCatalog[nativeGroup]?.[key]});
        });
      };
      search.addEventListener('input',e=>{nativeSearch=e.target.value;renderList();});renderList();
    } else if(tab==='json') {
      element('p',{class:'hint',text:t('rawHint')},body);
      if(!rawDirty)rawText=C.serialize(doc());
      element('textarea',{id:'raw-editor',class:'json-editor',value:rawText,spellcheck:false,'aria-label':t('json'),on:{input:e=>{rawText=e.target.value;rawDirty=rawText!==C.serialize(doc());renderHeader();}}},body);
      const actions=element('div',{class:'action-row'},body);
      element('button',{class:'primary',text:t('apply'),on:{click:applyRaw}},actions);
      element('button',{text:t('discard'),on:{click:()=>{rawDirty=false;rawText=C.serialize(doc());notice('');render();}}},actions);
    } else {
      ['helpSteps','helpSafety','helpKeys'].forEach(key=>element('p',{class:'hint help-block',text:t(key)},body));
    }
  }
  function renderInstrument(body) {
    element('p',{class:'hint',text:t('instrumentHint')},body);
    const base=mode==='common'?['gameplay']:['gameplay','modes',mode];
    const selected=C.gameplay(doc(),mode==='common'?'__common__':mode);
    const modeSelect=element('select',{class:'wide','aria-label':t('mode'),on:{change:e=>{
      if(!canChange()){e.target.value=mode;return;}mode=e.target.value;
      if(mode!=='common')previewMode=mode;render();
    }}},body);
    option(modeSelect,'common',t('common'));modes.filter(m=>m==='7+1'||parseInt(m,10)>=4).forEach(m=>option(modeSelect,m,m.toUpperCase()));modeSelect.value=mode;
    field(body,[...base,'renderer'],{type:'string',enum:['legacy','native']},{fallback:selected.renderer||'legacy'});
    if(selected.renderer!=='native')element('p',{class:'hint notice warning',text:t('enableInstrument')},body);
    const groupSelect=element('select',{class:'wide','aria-label':t('group'),on:{change:e=>{instrumentGroup=e.target.value;renderInspector();draw();}}},body);
    ['motion','sprites','metrics','colors','rects','fonts'].forEach(group=>option(groupSelect,group,t(group)));groupSelect.value=instrumentGroup;
    if(instrumentGroup==='sprites'){renderSpriteEditor(body,base,selected);return;}
    if(instrumentGroup==='rects')element('p',{class:'hint',text:t('deltaHint')},body);
    if(instrumentGroup==='metrics'||instrumentGroup==='fonts')element('p',{class:'hint',text:t('nativeTypographyHint')},body);
    const values=G.settings(selected,gameplayCatalog),list=element('div',{class:'native-list'},body);
    for(const [key,meta] of Object.entries(gameplayCatalog[instrumentGroup]||{})){
      const rule=instrumentGroup==='colors'?{$ref:'#/$defs/hexColor'}:
        instrumentGroup==='rects'?{$ref:'#/$defs/nativeRectAdjustment'}:
        instrumentGroup==='fonts'?{type:'string',maxLength:128}:
        {type:'number',minimum:meta.min,maximum:meta.max};
      field(list,[...base,'native',instrumentGroup,key],rule,{label:t('g_'+key)===`g_${key}`?meta.label:t('g_'+key),fallback:values[instrumentGroup][key]});
    }
  }
  function renderSpriteEditor(body,base,selected) {
    const select=element('select',{class:'wide','aria-label':t('spriteSlot'),on:{change:e=>{spriteSlot=e.target.value;spriteLayer=0;renderInspector();}}},body);
    Object.keys(gameplayCatalog.sprites||{}).forEach(key=>option(select,key,t(key)));select.value=spriteSlot;
    const meta=gameplayCatalog.sprites[spriteSlot];if(!meta)return;
    const path=[...base,'native','sprites',spriteSlot],layers=C.clone(selected.native?.sprites?.[spriteSlot]??meta.default);
    const heading=element('div',{class:'action-row'},body);
    element('button',{text:t('reset'),disabled:C.get(doc(),path)===undefined,on:{click:()=>change(path,undefined)}},heading);
    element('button',{text:t('hideSprite'),disabled:layers.length===0,on:{click:()=>change(path,[])}},heading);
    element('p',{class:'hint',text:t('spriteHint')+` · ${meta.width} × ${meta.height}`},body);
    const canvas=element('canvas',{class:'sprite-preview',width:meta.width,height:meta.height,'aria-label':t('spriteSlot')},body);
    const context=canvas.getContext('2d');
    G.drawSprite(context,layers,[meta.width,meta.height],[0,0,canvas.width,canvas.height],'#4F80FF');
    const list=element('select',{class:'wide','aria-label':t('spriteLayer'),on:{change:e=>{spriteLayer=Number(e.target.value);renderInspector();}}},body);
    layers.forEach((layer,index)=>option(list,String(index),`${index+1} · ${layer.color} · ${layer.width} × ${layer.height}`));
    spriteLayer=Math.max(0,Math.min(spriteLayer,layers.length-1));list.value=String(spriteLayer);list.disabled=!layers.length;
    const actions=element('div',{class:'action-row'},body);
    element('button',{text:t('addLayer'),disabled:layers.length>=128,on:{click:()=>{
      layers.push({x:8,y:8,width:meta.width-16,height:Math.max(4,meta.height-16),color:'#FFFFFF',mix:.2,alpha:1,radius:2});spriteLayer=layers.length-1;change(path,layers);
    }}},actions);
    element('button',{text:t('removeLayer'),disabled:!layers.length,on:{click:()=>{layers.splice(spriteLayer,1);spriteLayer=Math.max(0,spriteLayer-1);change(path,layers);}}},actions);
    for(const [delta,label]of[[-1,'layerBack'],[1,'layerForward']])element('button',{text:t(label),disabled:!layers.length||spriteLayer+delta<0||spriteLayer+delta>=layers.length,on:{click:()=>{
      [layers[spriteLayer],layers[spriteLayer+delta]]=[layers[spriteLayer+delta],layers[spriteLayer]];spriteLayer+=delta;change(path,layers);
    }}},actions);
    if(!layers.length){element('p',{class:'hint',text:t('spriteHidden')},body);return;}
    const layer=layers[spriteLayer],fields=element('div',{class:'layer-fields'},body);
    const numbers={x:[-256,256,1],y:[-512,512,1],width:[0,512,1],height:[0,512,1],mix:[0,1,.01],alpha:[0,1,.01],radius:[0,128,1]};
    const definition=C.resolve(schema.$defs[spriteSlot.startsWith('key_')?'nativeGameplayKeyLayer':'nativeGameplayNoteLayer']||{},schema);
    for(const key of ['x','y','width','height','color','mix','alpha','radius']){
      const row=element('label',{text:t('layer_'+key)},fields),rule=definition.properties?.[key]||{};
      const props=key==='color'?{type:'color',value:String(layer.color||'#FFFFFF').slice(0,7)}:{type:'number',min:rule.minimum??numbers[key][0],max:rule.maximum??numbers[key][1],step:numbers[key][2],value:layer[key]??0};
      element('input',{...props,'aria-label':t('layer_'+key),on:{change:e=>{
        const value=key==='color'?e.target.value.toUpperCase():e.target.valueAsNumber;
        if(key!=='color'&&!Number.isFinite(value))return;layers[spriteLayer][key]=value;change(path,layers);
      }}},row);
    }
  }
  function renderHeader() {
    document.documentElement.lang=language;document.title=`${doc().name||'TenRiff'} · ${t('app')}`;
    document.querySelectorAll('[data-i18n]').forEach(node=>node.textContent=t(node.dataset.i18n));
    $('language').value=language;$('skin-name').textContent=doc().name||t('newName');
    $('dirty-state').textContent=t(history.dirty||rawDirty?'dirty':'clean');
    $('undo').disabled=!history.canUndo||rawDirty;$('redo').disabled=!history.canRedo||rawDirty;
    $('preview').setAttribute('aria-label',t('preview'));$('tabs').setAttribute('aria-label',t('app'));
    $('tabs').replaceChildren();tabs.forEach((key,index)=>{
      const button=element('button',{class:tab===key?'active':'',type:'button','aria-current':tab===key?'page':'false',on:{click:()=>{tab=key;render();}}},$('tabs'));
      element('span',{class:'tab-index',text:String(index+1).padStart(2,'0')},button);element('span',{text:t(key)},button);
    });
    $('screen').replaceChildren();screens.forEach(key=>option($('screen'),key,t(key)));$('screen').value=screen;
    $('preview-mode').replaceChildren();modes.forEach(key=>option($('preview-mode'),key,key.toUpperCase()));$('preview-mode').value=previewMode;
    $('preview-mode').disabled=!['gameplay','instrument'].includes(tab);
  }
  function renderValidation() {
    const list=issues(), errors=list.filter(issue=>issue.severity==='error');
    $('validation-title').textContent=errors.length?`${t('errors')} ${errors.length}`:t('valid');
    $('validation-title').style.color=errors.length?'var(--red)':'var(--accent)';
    $('validation-count').textContent=`${t('warnings')} ${list.length-errors.length}`;
    $('issues').replaceChildren();
    list.slice(0,120).forEach(issue=>element('li',{class:issue.severity,text:`${issue.path||'JSON'} — ${t(issue.code)}${issue.detail!==undefined&&issue.detail!==null?' '+issue.detail:''}`},$('issues')));
  }
  function renderAssets() {
    $('asset-list').replaceChildren();
    if(!files.size)element('p',{class:'hint',text:t('noAssets')},$('asset-list'));
    for(const [key,entry]of images){
      const tile=element('div',{class:'asset-tile'},$('asset-list'));
      element('img',{src:entry.url,alt:key,loading:'lazy'},tile);element('span',{text:key,title:key},tile);
    }
  }
  function render(){
    const scroll=document.querySelector('.native-list')?.scrollTop||0;
    renderHeader();renderInspector();renderValidation();renderAssets();draw();
    const list=document.querySelector('.native-list');if(list)list.scrollTop=scroll;
  }
  function effectiveRect(targetScreen,key){
    const value=C.get(doc(),['layout',targetScreen,key]);if(Array.isArray(value)&&value.length===4&&value.every(Number.isFinite)&&value[2]>value[0]&&value[3]>value[1])return value;
    if(targetScreen==='title'&&doc().lobby?.renderer==='native'&&C.nativeTitleLayout[key])return [...C.nativeTitleLayout[key]];
    if(key==='avatar'&&(targetScreen==='song_select'||targetScreen==='result')){
      const profile=effectiveRect(targetScreen,'profile'),original=layouts[targetScreen].profile,avatar=layouts[targetScreen].avatar;
      return avatar.map((coordinate,index)=>coordinate+profile[index%2]-original[index%2]);
    }
    if(!layouts[targetScreen]){
      const parent=targetScreen.startsWith('settings_')?'settings':targetScreen==='song_browser'||targetScreen==='multiplayer'?'song_select':'generic';
      const inherited=C.get(doc(),['layout',parent,key])||C.get(doc(),['layout','generic',key]);
      if(Array.isArray(inherited)&&inherited.length===4&&inherited[2]>inherited[0]&&inherited[3]>inherited[1])return inherited;
    }
    return [...((layouts[targetScreen]||layouts.generic)[key]||[80,140,1840,1008])];
  }
  function palette(){
    const document=doc(),colors={...nativeCatalog.colors,...document.native?.colors},native=document.lobby?.renderer==='native';
    const mapped=native?{accent:colors['palette.63e9f2'],scene_background:colors['palette.05090f'],text:colors['palette.f7fafd'],panel:colors['palette.101d32'],card:'#090909',footer:'#080808',button:'#090909',button_selected:'#111111',border:colors['options.border'],muted:'#999999',scene_primary:'#000000',scene_secondary:'#000000'}:{};
    return Object.fromEntries(Object.entries({...defaults.theme,...mapped,...document.theme}).map(([key,value])=>[key,/^#?[0-9a-f]{6}([0-9a-f]{2})?$/i.test(value)?'#'+value.replace(/^#/,''):defaults.theme[key]]));
  }
  const ctx=$('preview').getContext('2d');
  function box(rect,fill,stroke,radius=14,strokeWidth=2){
    if(!rect||!rect.every(Number.isFinite)||rect[2]<=rect[0]||rect[3]<=rect[1])return;
    ctx.beginPath();const w=rect[2]-rect[0],h=rect[3]-rect[1],r=Math.min(radius,w/2,h/2);
    if(ctx.roundRect)ctx.roundRect(rect[0],rect[1],w,h,r);else ctx.rect(rect[0],rect[1],w,h);
    if(fill){ctx.fillStyle=fill;ctx.fill();}if(stroke){ctx.strokeStyle=stroke;ctx.lineWidth=strokeWidth;ctx.stroke();}
  }
  function text(value,x,y,size=24,color=palette().text,maxWidth=1500){
    ctx.fillStyle=color;ctx.font=`600 ${size}px "Segoe UI", "Yu Gothic UI", sans-serif`;ctx.textBaseline='middle';ctx.fillText(value,x,y,maxWidth);
  }
  function imageFor(path){return typeof path==='string'?images.get(C.normalizeAsset(path).toLowerCase())?.image:null;}
  function art(path,rect,opacity=1,contain=false){
    const img=imageFor(path);if(!img?.complete||!img.naturalWidth)return false;
    ctx.save();ctx.globalAlpha=Math.max(0,Math.min(1,opacity));
    if(contain){const scale=Math.min((rect[2]-rect[0])/img.naturalWidth,(rect[3]-rect[1])/img.naturalHeight),w=img.naturalWidth*scale,h=img.naturalHeight*scale;ctx.drawImage(img,(rect[0]+rect[2]-w)/2,(rect[1]+rect[3]-h)/2,w,h);}
    else ctx.drawImage(img,rect[0],rect[1],rect[2]-rect[0],rect[3]-rect[1]);
    ctx.restore();return true;
  }
  function background(now){
    const p=palette(),document=doc(),lobby=document.lobby||{};
    ctx.fillStyle=p.scene_background;ctx.fillRect(0,0,1920,1080);
    if(lobby.renderer!=='native'){
      const grad=ctx.createRadialGradient(1300,100,0,1300,100,1500);grad.addColorStop(0,p.scene_secondary.slice(0,7)+'22');grad.addColorStop(1,'#00000000');
      ctx.fillStyle=grad;ctx.fillRect(0,0,1920,1080);
    }
    let path=lobby.screen_backgrounds?.[screen];if(!path&&screen.startsWith('settings'))path=lobby.screen_backgrounds?.settings;
    path=path||lobby.background||'lobby/background.png';art(path,[0,0,1920,1080],lobby.screen_opacities?.[screen]??lobby.background_opacity??.72);
    if(lobby.renderer==='native')return;
    ctx.save();ctx.globalAlpha=.16;ctx.strokeStyle=p.accent;ctx.lineWidth=1;
    for(let i=0;i<7;i++){const shift=$('animate').checked?Math.sin(now/2300+i)*20:0;ctx.beginPath();ctx.moveTo(1300+i*70+shift,0);ctx.lineTo(850+i*140+shift,1080);ctx.stroke();}ctx.restore();
  }
  function drawTitle(now){
    if(doc().lobby?.renderer==='native'){drawNativeTitle(now);return;}
    const p=palette(),r=key=>effectiveRect('title',key),logo=r('logo'),spec=r('spectrum');
    if(!art(doc().lobby?.logo||'lobby/logo.png',logo,1,true))text('TenRiff',logo[0]+18,(logo[1]+logo[3])/2,100,p.text,logo[2]-logo[0]-36);
    const bw=(spec[2]-spec[0])/36;for(let i=0;i<18;i++){const wave=$('animate').checked?Math.sin(now/260+i*.72):Math.sin(i*.72);const h=(spec[3]-spec[1])*(.25+.7*Math.abs(wave));box([spec[0]+i*bw*2,spec[3]-h,spec[0]+i*bw*2+bw,spec[3]],i%3?p.accent:p.scene_secondary,null,3);}
    const buttons=r('buttons'),labels=['start','multiplayer','options','help'];const height=(buttons[3]-buttons[1]-48)/4;
    labels.forEach((label,i)=>{const y=buttons[1]+i*(height+16);box([buttons[0],y,buttons[2],y+height],i===0?p.button_selected:p.button,p.border,18);text(String(i+1).padStart(2,'0'),buttons[0]+32,y+height/2,22,p.accent);text(t(label),buttons[0]+92,y+height/2,35,p.text,buttons[2]-buttons[0]-150);});
    const guide=r('guide');box(guide,p.panel,p.border);text(t('guide'),guide[0]+24,guide[1]+35,22,p.accent,guide[2]-guide[0]-48);text('♪  TENRIFF',guide[0]+24,guide[1]+100,24,p.text,guide[2]-guide[0]-48);
    const footer=r('footer');box(footer,p.footer,p.border);text('TENRIFF  /  '+t('start'),footer[0]+24,(footer[1]+footer[3])/2,20,p.muted,footer[2]-footer[0]-48);
  }
  function drawNativeTitle(now){
    const p=palette(),document=doc(),r=key=>effectiveRect('title',key),logo=r('logo');
    const moving=$('animate').checked&&(document.native?.motion?.enabled??1)>0;
    const time=now*Math.max(0,Math.min(4,document.native?.motion?.speed??1));
    box([0,0,1920,126],p.panel,p.border,0);
    if(!art(document.lobby?.logo||'lobby/logo.png',logo,1,true)){
      const size=Math.min(logo[3]-logo[1],logo[2]-logo[0]);
      if(!art(document.native?.assets?.mark,[logo[0],logo[1],logo[0]+size,logo[3]],1,true)){
        ctx.save();ctx.translate(logo[0]+size/2,(logo[1]+logo[3])/2);ctx.rotate(-Math.PI/8);box([-size*.32,-size*.32,size*.32,size*.32],p.accent,null,6);ctx.restore();
      }
      text('TENRIFF',logo[0]+size+16,(logo[1]+logo[3])/2,52,p.text,Math.max(20,logo[2]-logo[0]-size-16));
    }
    text(t('title'),466,64,25,p.muted);box([1424,20,1824,110],p.card,p.border,12);text(t('profile'),1448,43,17,p.muted);text('PLAYER',1448,79,24,p.text);
    text('BMS RHYTHM GAME',96,212,18,p.accent);text(t('yourMusic'),96,294,52,p.text,568);text(t('yourRhythm'),96,374,52,p.accent,568);text(t('nextPlay'),98,458,21,p.muted,586);
    const float=moving?Math.sin(time/870)*7*Math.max(0,Math.min(2,document.native?.motion?.intensity??1)):0;
    if(!art(document.native?.assets?.prism,[708,250+float,892,458+float],.85,true)){
      ctx.save();ctx.translate(800,354+float);ctx.rotate(Math.PI/4);box([-66,-66,66,66],p.button_selected,p.accent,10);box([-42,-42,42,42],null,p.scene_secondary,6);ctx.restore();
    }
    const guide=r('guide');box(guide,p.panel,p.border,16);text(t('guide'),guide[0]+30,guide[1]+38,26,p.accent,guide[2]-guide[0]-60);
    ['song_select','settings_input','settings_skins'].forEach((key,index)=>{
      const y=guide[1]+98+index*(guide[3]-guide[1]-150)/3;
      text(String(index+1).padStart(2,'0'),guide[0]+32,y,21,p.accent);text(t(key),guide[0]+90,y,26,p.text,guide[2]-guide[0]-122);
    });
    const buttons=r('buttons'),scale=(buttons[3]-buttons[1])/564;let y=buttons[1];
    text(t('start'),buttons[0],buttons[1]-40,19,p.muted,buttons[2]-buttons[0]);
    ['start','multiplayer','options','exit'].forEach((key,index)=>{
      const h=(index===0?168:114)*scale;box([buttons[0],y,buttons[2],y+h],index===0?p.accent:p.card,p.border,14);
      text(t(key),buttons[0]+28,y+h*.4,34,index===0?'#0B1620':p.text,buttons[2]-buttons[0]-124);
      text(index===0?t('song_select'):'TENRIFF',buttons[0]+30,y+h*.75,19,index===0?'#234652':p.muted,buttons[2]-buttons[0]-124);
      text('→',buttons[2]-75,y+h/2,40,index===0?'#0B1620':p.accent);y+=h+18*scale;
    });
    const footer=r('footer');box(footer,p.footer,p.border,12);text(t('selectedTrack'),footer[0]+24,(footer[1]+footer[3])/2,20,p.muted,250);text('SAMPLE TRACK',footer[0]+276,(footer[1]+footer[3])/2,25,p.text,footer[2]-footer[0]-730);
    const spec=r('spectrum'),bar=(spec[2]-spec[0])/36;
    for(let i=0;i<18;i++){const wave=moving?Math.sin(time/260+i*.72):Math.sin(i*.72),height=(spec[3]-spec[1])*(.25+.7*Math.abs(wave));box([spec[0]+i*bar*2,spec[3]-height,spec[0]+i*bar*2+bar,spec[3]],i%3?p.accent:p.scene_secondary,null,2);}
    text('F1  '+t('help'),footer[2]-244,(footer[1]+footer[3])/2,20,p.muted,220);
  }
  function drawSelect(){
    const p=palette(),r=key=>effectiveRect('song_select',key);box(r('top_bar'),p.panel,p.border,0);
    const logo=r('logo');if(!art(doc().lobby?.logo||'lobby/logo.png',logo,1,true))text('TenRiff',logo[0],(logo[1]+logo[3])/2,68,p.text,logo[2]-logo[0]);
    const nav=r('nav');text(t('collection')+'   /   '+t('library'),nav[0]+24,(nav[1]+nav[3])/2,28,p.accent,nav[2]-nav[0]-48);
    const profile=r('profile');box(profile,p.card,p.border);text('PLAYER',profile[0]+112,(profile[1]+profile[3])/2,25,p.text,profile[2]-profile[0]-140);box(r('avatar'),p.button_selected,p.accent,20);
    for(const name of ['left_panel','center_panel','right_panel'])box(r(name),p.panel,p.border);
    const left=r('left_panel');text(t('library'),left[0]+26,left[1]+42,26,p.text,left[2]-left[0]-50);for(let i=0;i<6;i++){box([left[0]+20,left[1]+90+i*78,left[2]-20,left[1]+148+i*78],i===0?p.button_selected:p.card,null,8);text(i===0?t('collection'):'COLLECTION 0'+i,left[0]+40,left[1]+119+i*78,21,p.muted,left[2]-left[0]-80);}
    const center=r('center_panel');text(t('song_select'),center[0]+24,center[1]+38,25,p.text,center[2]-center[0]-48);
    for(let i=0;i<6;i++){const y=center[1]+84+i*101;if(y+88>center[3])break;box([center[0]+18,y,center[2]-18,y+86],i===2?p.button_selected:p.card,i===2?p.accent:p.border,10);text('0'+(i+1),center[0]+40,y+43,29,p.accent);text(i===2?'PRISMATIC VELOCITY':'SAMPLE TRACK '+(i+1),center[0]+106,y+33,27,p.text,center[2]-center[0]-144);text('TENRIFF  /  '+previewMode.toUpperCase(),center[0]+106,y+65,17,p.muted);}
    const right=r('right_panel');text('PRISMATIC',right[0]+28,right[1]+55,38,p.text,right[2]-right[0]-56);box([right[0]+28,right[1]+108,right[2]-28,right[1]+360],p.button_selected,p.border,14);text('LV 12',right[0]+55,right[1]+232,70,p.accent,right[2]-right[0]-110);text('HI-SPEED  3.20',right[0]+30,right[1]+430,25,p.muted);text('BEST  98.72%',right[0]+30,right[1]+500,30,p.text,right[2]-right[0]-60);
    const bottom=r('bottom_bar');box(bottom,p.footer,p.border);text('7K  /  NORMAL',bottom[0]+30,(bottom[1]+bottom[3])/2,25,p.muted);text(t('start')+'  →',bottom[2]-280,(bottom[1]+bottom[3])/2,32,p.accent,240);
  }
  function drawResult(){
    const p=palette(),r=key=>effectiveRect('result',key);text(t('result').toUpperCase(),70,72,48,p.text);
    for(const key of Object.keys(layouts.result)){const rect=r(key);box(rect,key==='continue'?p.button_selected:p.panel,p.border);if(key!=='avatar')text(t(key),rect[0]+22,rect[1]+32,23,p.muted,rect[2]-rect[0]-44);}
    const song=r('song_panel');text('SAMPLE TRACK',song[0]+28,song[1]+126,34,p.text,song[2]-song[0]-56);text('AAA',710,340,140,p.accent);text('987,654',650,500,74,p.text);text('98.76%',725,608,45,p.muted);
    const analysis=r('analysis_panel');ctx.strokeStyle=p.accent;ctx.lineWidth=4;ctx.beginPath();for(let i=0;i<28;i++){const x=analysis[0]+28+(analysis[2]-analysis[0]-56)*i/27,y=analysis[3]-60-(analysis[3]-analysis[1]-170)*(.25+i/40+.06*Math.sin(i));if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
  }
  const optionRoles=['key_mode','keymap','skin','graphics','audio','input','latency','profile','mode','key_test'];
  const optionAssets=['keys','keymap','skin','display','audio','input','latency','profile','sliders','keytest'];
  function optionMetric(key,low,high){
    const value=doc().native?.metrics?.[key]??nativeCatalog.metrics[key];
    return Math.max(low,Math.min(high,value));
  }
  function optionRect(index){
    const r=effectiveRect('options','content'),gap=optionMetric('options_grid.gap',0,200),height=optionMetric('options_grid.height',1,800);
    const width=(r[2]-r[0]-gap*4)/5,x=r[0]+index%5*(width+gap),y=r[1]+40+Math.floor(index/5)*(height+gap);
    return G.adjusted([x,y,x+width,y+height],doc().native?.rects?.['options_grid.rect.001']);
  }
  function drawOptions(){
    const p=palette(),colors={...nativeCatalog.colors,...doc().native?.colors};
    const labels=['optionKeys','keymap','settings_skins','settings_graphics','settings_audio','settings_input','settings_calibration','profile','mode_mods','keymap_test'];
    const values=[previewMode.toUpperCase(),t('keymap'),'Native','1920 × 1080','WASAPI','RawInput','0.0 ms','default','MOD',t('keymap_test')];
    box([0,0,1920,126],p.panel,p.border,0);text('TENRIFF',64,64,44,p.text);text(t('options'),430,54,30,p.text);
    optionRoles.forEach((role,index)=>{
      const r=optionRect(index),chosen=index===selectedOption,ink=colors['options.icon.'+role],accent=colors['options.'+role];
      const tint=optionMetric(chosen?'options_grid.selected_tint':'options_grid.tint',0,.65);
      const fill='#'+[1,3,5].map(offset=>Math.round((.035+parseInt(accent.slice(offset,offset+2),16)/255*tint)*255).toString(16).padStart(2,'0')).join('');
      box(r,fill,chosen?ink:colors['options.border'],optionMetric('options_grid.radius',0,32),optionMetric(chosen?'options_grid.selected_border_width':'options_grid.border_width',.5,8));
      box(G.adjusted([r[0]+24,r[1]+26,r[0]+68,r[1]+30],doc().native?.rects?.['options_grid.rect.002']),chosen?accent:'#404040',null,0);
      text(t(labels[index]),r[0]+24,r[1]+68,22,p.muted,r[2]-r[0]-90);
      text(values[index],r[0]+24,r[3]-62,38,colors['options.value'],r[2]-r[0]-48);
      const asset=optionAssets[index],rect=G.adjusted([r[2]-72,r[1]+28,r[2]-24,r[1]+76],doc().native?.rects?.['options_grid.rect.005']);
      // PNG pixels keep their authored colors, matching the native renderer.
      if(!art(doc().native?.assets?.[asset],rect,1,true)){
        ctx.save();ctx.translate(rect[0],rect[1]);ctx.scale((rect[2]-rect[0])/100,(rect[3]-rect[1])/100);
        ctx.strokeStyle=ink;ctx.fillStyle=ink;ctx.lineWidth=optionMetric('icon.stroke_width',.5,8);ctx.lineJoin='round';
        for(const [,filled,points] of catalog.menuVectors?.[asset]||[]){ctx.beginPath();points.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));if(filled)ctx.fill();else ctx.stroke();}
        ctx.restore();
      }
    });
    text(t('optionPreviewHint'),64,960,21,p.muted,1792);
  }
  function drawGeneric(){
    if(screen==='options'&&doc().lobby?.renderer==='native'){drawOptions();return;}
    const p=palette(),r=effectiveRect(screen,'content');text(t(screen),65,84,48,p.text);box(r,p.panel,p.border);
    const preview=effectiveRect(screen,'preview'),hasPreview=screen==='settings_skins';
    const right=hasPreview?Math.min(preview[0]-22,r[2]-24):r[2]-24;
    for(let i=0;i<7;i++){const y=r[1]+32+i*94;if(y+74>r[3])break;box([r[0]+24,y,right,y+74],i===2?p.button_selected:p.card,p.border,10);text([t('theme'),t('layout'),t('native'),t('background'),t('mode'),t('settings_audio'),t('settings_input')][i],r[0]+46,y+37,27,p.text,right-r[0]-100);}
    if(hasPreview){box(preview,p.card,p.accent);text(t('preview'),preview[0]+24,preview[1]+40,29,p.accent,preview[2]-preview[0]-48);}
  }
  function drawGameplay(now=0){
    const p=palette(),style={...defaults.gameplay,...C.gameplay(doc(),previewMode)},count=previewMode==='7+1'?8:parseInt(previewMode,10);
    if(style.renderer==='native' || (doc().lobby?.renderer==='native'&&!doc().gameplay)) {
      G.paint(ctx,style,gameplayCatalog,previewMode,now||performance.now(),$('animate').checked,art,pressedPreviewLane);return;
    }
    art(style.background||'gameplay/background.png',[0,0,1920,1080],style.background_opacity);
    const width=Math.min(1200,count*95),left=(1920-width)/2,line=1080*style.judgement_line_position,laneWidth=width/count;
    box([left,50,left+width,1060],style.black_playfield?'#000000ee':p.panel,p.border,0);
    for(let i=0;i<count;i++){
      const x=left+i*laneWidth,color=style.lane_colors?.[i%style.lane_colors.length]||(i%2?p.accent:p.text);
      ctx.save();ctx.globalAlpha=Math.max(0,Math.min(1,style.lane_background_opacity));box([x,50,x+laneWidth,1060],color,null,0);ctx.restore();
      if(style.show_lane_dividers){ctx.strokeStyle=p.lane_divider;ctx.globalAlpha=.2;ctx.beginPath();ctx.moveTo(x,50);ctx.lineTo(x,1060);ctx.stroke();ctx.globalAlpha=1;}
      const pressed=i===pressedPreviewLane;
      const backdrop=G.backdropOpacity(style,pressed),backdropTop=G.backdropTop(style,50,1060);
      if(backdrop>0&&backdropTop<1060){ctx.save();ctx.globalAlpha=backdrop;box([x+1,backdropTop,x+laneWidth-1,1060],G.backdropColor(color,style),null,0);ctx.restore();}
      const receptor=[x+5,line-18,x+laneWidth-5,line+18];if(!art(C.laneAsset(style.key_idle,i,style.lane_map),receptor))box(receptor,p.button_selected,p.accent,4);
      for(let j=0;j<3;j++){
        const y=120+((i*97+j*235)%Math.max(100,line-220)),w=laneWidth*.86*style.note_width_ratio,h=18*style.note_height_ratio,rect=[x+(laneWidth-w)/2,y,x+(laneWidth+w)/2,y+h];
        ctx.globalAlpha=style.visual_opacity;
        if(!art(C.laneAsset(style.note,i,style.lane_map),rect,style.visual_opacity,style.note_aspect==='contain')){
          if(style.note_shape==='circle'){ctx.fillStyle=color;ctx.beginPath();ctx.ellipse(x+laneWidth/2,y+h/2,w/2,h/2,0,0,Math.PI*2);ctx.fill();}
          else if(style.note_shape==='diamond'||style.note_shape==='hex'){ctx.fillStyle=color;ctx.beginPath();ctx.moveTo(rect[0],y+h/2);ctx.lineTo(rect[0]+w*.2,y);ctx.lineTo(rect[2]-w*.2,y);ctx.lineTo(rect[2],y+h/2);ctx.lineTo(rect[2]-w*.2,y+h);ctx.lineTo(rect[0]+w*.2,y+h);ctx.closePath();ctx.fill();}
          else box(rect,color,style.note_border?p.text:null,3);
        }ctx.globalAlpha=1;
      }
      if(style.key_label_position!=='off')text(String(i+1),x+laneWidth*.4,line+(style.key_label_position==='top'?-44:52),22,p.muted,laneWidth);
    }
    if(style.show_judgement_line){const thickness=4*Math.max(.25,Math.min(4,style.note_height_ratio??1));box([left,line-thickness/2,left+width,line+thickness/2],p.judgement_line,null,0);}
    art(style.gear||'gameplay/gear.png',[left,50,left+width,1060]);text(previewMode.toUpperCase(),left,24,22,p.accent);
  }
  function draw(time=0){
    if(!ctx)return;
    ctx.clearRect(0,0,1920,1080);ctx.globalAlpha=1;
    try{
      background(time);
      if(tab==='gameplay'||tab==='instrument')drawGameplay(time);else if(screen==='title')drawTitle(time);else if(screen==='song_select')drawSelect();else if(screen==='result')drawResult();else drawGeneric();
      if($('grid').checked){ctx.save();ctx.strokeStyle='#ffffff12';ctx.lineWidth=1;ctx.beginPath();for(let x=0;x<=1920;x+=80){ctx.moveTo(x,0);ctx.lineTo(x,1080);}for(let y=0;y<=1080;y+=80){ctx.moveTo(0,y);ctx.lineTo(1920,y);}ctx.stroke();ctx.restore();}
      if(tab==='layout'&&$('outlines').checked){
        for(const key of Object.keys(layouts[screen]||layouts.generic)){
          const r=drag&&drag.slot===key?drag.rect:effectiveRect(screen,key),selected=key===slot;
          ctx.save();ctx.strokeStyle=selected?'#71ffe0':'#b6cee577';ctx.lineWidth=selected?3:1;ctx.setLineDash(selected?[]:[8,6]);ctx.strokeRect(r[0],r[1],r[2]-r[0],r[3]-r[1]);ctx.setLineDash([]);
          box([r[0],r[1],r[0]+Math.min(270,r[2]-r[0]),r[1]+32],selected?'#184339':'#142231',null,0);text(t(key),r[0]+8,r[1]+16,18,selected?'#a7ffe9':'#bdcedf',Math.min(254,r[2]-r[0]-16));
          if(selected)box([r[2]-13,r[3]-13,r[2]+13,r[3]+13],'#71ffe0','#173e34',3);ctx.restore();
        }
      }
    }catch(_){/* Invalid draft values are reported by validation; keep the editor usable. */}
  }
  function tick(time){draw(time);raf=$('animate').checked&&!document.hidden?requestAnimationFrame(tick):0;}
  function animation(){if(raf)cancelAnimationFrame(raf);raf=0;if($('animate').checked&&!document.hidden)raf=requestAnimationFrame(tick);else draw();}
  function point(event){const r=$('preview').getBoundingClientRect();return[(event.clientX-r.left)*1920/r.width,(event.clientY-r.top)*1080/r.height];}
  $('preview').addEventListener('pointerdown',event=>{
    if(['gameplay','instrument'].includes(tab)&&event.button===0){const [x]=point(event),count=previewMode==='7+1'?8:parseInt(previewMode,10);pressedPreviewLane=x>=470&&x<1450?Math.floor((x-470)/980*count):-1;$('preview').setPointerCapture(event.pointerId);draw();return;}
    if(screen==='options'&&doc().lobby?.renderer==='native'&&tab!=='layout'&&event.button===0){
      const [x,y]=point(event),index=optionRoles.findIndex((_,i)=>{const r=optionRect(i);return x>=r[0]&&x<=r[2]&&y>=r[1]&&y<=r[3];});
      if(index>=0){selectedOption=index;draw();}return;
    }
    if(tab!=='layout'||!canChange()||event.button!==0)return;
    const [x,y]=point(event),r=effectiveRect(screen,slot),scale=1920/$('preview').getBoundingClientRect().width,handle=8*scale;
    let resize=Math.abs(x-r[2])<handle&&Math.abs(y-r[3])<handle;
    let hit=resize?slot:Object.keys(layouts[screen]||layouts.generic).reverse().find(key=>{const q=effectiveRect(screen,key);return x>=q[0]&&x<=q[2]&&y>=q[1]&&y<=q[3];});
    if(!hit)return;slot=hit;const original=effectiveRect(screen,slot);drag={slot,start:[x,y],original,rect:original,resize};$('preview').setPointerCapture(event.pointerId);renderInspector();draw();
  });
  $('preview').addEventListener('pointermove',event=>{
    const [x,y]=point(event);$('cursor-position').textContent=`${Math.round(x)}, ${Math.round(y)}`;
    if(!drag)return;drag.rect=C.moveRect(drag.original,x-drag.start[0],y-drag.start[1],drag.resize,$('snap').checked?8:1);draw();
  });
  $('preview').addEventListener('pointerup',()=>{pressedPreviewLane=-1;if(!drag){draw();return;}const complete=drag;drag=null;if(JSON.stringify(complete.rect)!==JSON.stringify(complete.original))change(['layout',screen,complete.slot],complete.rect);else draw();});
  $('preview').addEventListener('pointercancel',()=>{pressedPreviewLane=-1;drag=null;draw();});
  $('preview').addEventListener('keydown',event=>{
    if(tab!=='layout'||!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(event.key)||!canChange())return;
    event.preventDefault();const amount=event.shiftKey?10:1;change(['layout',screen,slot],C.moveRect(effectiveRect(screen,slot),event.key==='ArrowLeft'?-amount:event.key==='ArrowRight'?amount:0,event.key==='ArrowUp'?-amount:event.key==='ArrowDown'?amount:0,false));$('preview').focus();
  });
  function applyRaw(){
    try{const next=C.parse(rawText);const errors=issues(next).filter(item=>item.severity==='error');
      if(errors.length){notice(`${t('invalidSave')}\n${errors.slice(0,8).map(issue=>`${issue.path}: ${t(issue.code)} ${issue.detail??''}`).join('\n')}`,'error');return false;}
      history.commit(next);rawDirty=false;notice(t('valid'));render();return true;
    }catch(error){notice(`${t('json')}: ${t(error.message)}`,'error');return false;}
  }
  function confirmDiscard(){return!(history.dirty||rawDirty)||window.confirm(t('discardConfirm'));}
  function clearFiles(){for(const value of images.values())URL.revokeObjectURL(value.url);files.clear();images.clear();}
  function newSkin(template='native'){
    if(!confirmDiscard())return;const next=C.clone(template==='example'?catalog.example:(catalog.native||fallbackNative));next.name=t('newName');delete next.author;
    history.load(next);documentGeneration++;rawDirty=false;clearFiles();notice('');render();
  }
  function addFile(path,file){
    const normalized=C.normalizeAsset(path),key=normalized.toLowerCase();if(!C.safeAsset(normalized))return;
    if(file.size>64*1024*1024)throw new Error(t('tooLarge'));
    if(images.has(key))URL.revokeObjectURL(images.get(key).url);
    files.set(key,{path:normalized,file});const url=URL.createObjectURL(file),image=new Image();
    image.onload=()=>draw();image.onerror=()=>notice(`${t('readError')} ${normalized}`,'warning');image.src=url;images.set(key,{image,url,path:normalized});
  }
  async function readManifest(file){if(file.size>8*1024*1024)throw new Error(t('tooLarge'));return C.parse(await file.text());}
  async function loadJSON(file){
    try{const value=await readManifest(file);if(!confirmDiscard())return;history.load(value);documentGeneration++;rawDirty=false;clearFiles();notice(t('fileOnly'),'warning');render();}
    catch(error){notice(`${t('readError')} ${t(error.message)}`,'error');}
  }
  async function loadFolder(fileList){
    try{
      const manifests=fileList.filter(file=>/(^|\/)skin\.json$/i.test(file.webkitRelativePath||file.name));
      if(manifests.length!==1)throw new Error(t(manifests.length?'manyManifests':'noManifest'));
      const manifest=manifests[0],value=await readManifest(manifest),base=manifest.webkitRelativePath.slice(0,-manifest.name.length);
      const assets=fileList.filter(file=>file.webkitRelativePath.startsWith(base)&&C.safeAsset(file.webkitRelativePath.slice(base.length)));
      if(assets.some(file=>file.size>64*1024*1024))throw new Error(t('tooLarge'));
      if(!confirmDiscard())return;history.load(value);documentGeneration++;rawDirty=false;clearFiles();assets.forEach(file=>addFile(file.webkitRelativePath.slice(base.length),file));notice(t('opened'));render();
    }catch(error){notice(t(error.message),'error');}
  }
  function download(blob,name){const url=URL.createObjectURL(blob),a=element('a',{href:url,download:name},document.body);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);}
  function readyToSave(){if(rawDirty&&!applyRaw())return false;if(issues().some(item=>item.severity==='error')){notice(t('invalidSave'),'error');return false;}return true;}
  function save(){if(!readyToSave())return;download(new Blob([C.serialize(doc())],{type:'application/json;charset=utf-8'}),'skin.json');history.markSaved();notice(t('saved'));renderHeader();}
  async function exportZIP(){
    if(busy||!readyToSave())return;
    // Keep the saved marker tied to the exported snapshot if the user edits while files are read.
    const snapshot=doc(),assetSnapshot=[...files.values()],generation=documentGeneration;
    const missing=C.assetReferences(snapshot).filter(path=>!files.has(path.toLowerCase()));
    if(missing.length){notice(`${t('missingImages')}\n${missing.slice(0,18).join('\n')}`,'error');return;}
    busy=true;$('export').disabled=true;
    try{
      let folder=(snapshot.name||'TenRiff-Skin').replace(/[<>:"/\\|?*\x00-\x1f]/g,'-').replace(/^[. ]+|[. ]+$/g,'').slice(0,100)||'TenRiff-Skin';
      if(/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(folder))folder='Skin-'+folder;
      const entries=[{name:`${folder}/skin.json`,data:new TextEncoder().encode(C.serialize(snapshot))}];let total=entries[0].data.length;
      for(const entry of assetSnapshot){total+=entry.file.size;if(total>512*1024*1024)throw new Error('ZIP exceeds 512 MB');entries.push({name:`${folder}/${entry.path}`,data:new Uint8Array(await entry.file.arrayBuffer())});}
      download(new Blob(window.TenRiffZip.create(entries),{type:'application/zip'}),`${folder}.zip`);if(generation===documentGeneration)history.markSaved(snapshot);notice(t('saved'));renderHeader();
    }catch(error){notice(`${t('export')}: ${error.message}`,'error');}finally{busy=false;$('export').disabled=false;}
  }
  $('language').addEventListener('change',e=>{language=e.target.value;try{localStorage.setItem('tenriff-skin-editor-language',language);}catch(_){}notice('');render();});
  $('screen').addEventListener('change',e=>{screen=e.target.value;slot=Object.keys(layouts[screen]||layouts.generic)[0];renderInspector();draw();});
  $('preview-mode').addEventListener('change',e=>{previewMode=e.target.value;draw();});
  ['outlines','grid','snap'].forEach(id=>$(id).addEventListener('change',()=>draw()));$('animate').addEventListener('change',animation);document.addEventListener('visibilitychange',animation);
  $('new').addEventListener('click',()=>newSkin());$('open').addEventListener('click',()=>$('file-input').click());$('folder').addEventListener('click',()=>$('folder-input').click());
  $('file-input').addEventListener('change',async e=>{if(e.target.files[0])await loadJSON(e.target.files[0]);e.target.value='';});
  $('folder-input').addEventListener('change',async e=>{if(e.target.files.length)await loadFolder([...e.target.files]);e.target.value='';});
  $('add-images').addEventListener('click',()=>{imageTarget=null;$('image-input').multiple=true;$('image-input').click();});
  $('image-input').addEventListener('change',e=>{try{for(const file of e.target.files)addFile(file.name,file);if(imageTarget&&e.target.files[0])change(imageTarget,e.target.files[0].name);else{renderAssets();draw();}}catch(error){notice(error.message,'error');}e.target.value='';imageTarget=null;});
  $('undo').addEventListener('click',()=>{if(canChange()){history.undo();render();}});$('redo').addEventListener('click',()=>{if(canChange()){history.redo();render();}});$('save').addEventListener('click',save);$('export').addEventListener('click',exportZIP);
  document.addEventListener('keydown',event=>{
    if(!(event.ctrlKey||event.metaKey)||event.altKey)return;
    const key=event.key.toLowerCase(),editing=['INPUT','TEXTAREA','SELECT'].includes(document.activeElement?.tagName);
    if(key==='s'){event.preventDefault();save();}
    else if(key==='o'){event.preventDefault();$('file-input').click();}
    else if((key==='z'||key==='y')&&!editing){event.preventDefault();if(canChange()){if(key==='y'||event.shiftKey)history.redo();else history.undo();render();}}
  });
  window.addEventListener('beforeunload',event=>{if(history.dirty||rawDirty){event.preventDefault();event.returnValue='';}});
  render();
})();
