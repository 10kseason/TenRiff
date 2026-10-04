/* Native gameplay preview uses the same sprite layers and bounded style catalog as the client. */
(function(root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.TenRiffGameplay = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function() {
  'use strict';
  const clamp = (n, low, high) => Math.max(low, Math.min(high, n));
  const groups = ['metrics','colors','motion','rects','fonts','sprites'];
  function settings(style, catalog) {
    const output = {};
    for (const group of groups) {
      output[group] = {};
      for (const [key, meta] of Object.entries(catalog[group] || {})) {
        let value = style.native?.[group]?.[key] ?? meta.default;
        if (typeof meta.default === 'number')
          value = Number.isFinite(value) ? clamp(value, meta.min, meta.max) : meta.default;
        output[group][key] = value;
      }
    }
    return output;
  }
  function blend(a, b, mix) {
    const channels = color => [1,3,5].map(i => parseInt(String(color).slice(i,i+2),16) || 0);
    const left=channels(a),right=channels(b),amount=clamp(mix,0,1);
    return '#' + left.map((v,i)=>Math.round(v+(right[i]-v)*amount).toString(16).padStart(2,'0')).join('');
  }
  function advance(current, pressed, seconds, motion) {
    const target = pressed ? 1 : 0;
    if (!Number.isFinite(current) || !Number.isFinite(seconds) || seconds > .25) return target;
    return clamp(current + (target-current)*(1-Math.exp(-Math.max(0,seconds)*(pressed?motion.press_response:motion.release_response))),0,1);
  }
  function adjusted(rect, delta) {
    if (!Array.isArray(delta) || delta.length!==4 || !delta.every(Number.isFinite)) return rect;
    return [rect[0]+delta[0],rect[1]+delta[1],rect[0]+delta[0]+Math.max(1,rect[2]-rect[0]+delta[2]),rect[1]+delta[1]+Math.max(1,rect[3]-rect[1]+delta[3])];
  }
  function drawSprite(ctx, layers, size, rect, laneColor, opacity=1, outline=1) {
    if (!Array.isArray(layers)) return;
    ctx.save();ctx.translate(rect[0],rect[1]);ctx.scale((rect[2]-rect[0])/size[0],(rect[3]-rect[1])/size[1]);
    ctx.beginPath();ctx.rect(0,0,size[0],size[1]);ctx.clip();
    layers.slice(0,128).forEach((layer,index)=>{
      if (![layer.x,layer.y,layer.width,layer.height].every(Number.isFinite)) return;
      ctx.globalAlpha=clamp((layer.alpha??1)*opacity*(size[1]===32&&index<2?outline:1),0,1);
      ctx.fillStyle=blend(laneColor,layer.color||'#FFFFFF',layer.mix??1);
      ctx.beginPath();ctx.roundRect(layer.x,layer.y,Math.max(0,layer.width),Math.max(0,layer.height),Math.max(0,layer.radius||0));ctx.fill();
    });ctx.restore();
  }
  function noteShapeVertices(shape) {
    if(shape==='arrow')return [[0,-.5],[.5,-.04],[.18,-.04],[.18,.5],[-.18,.5],[-.18,-.04],[-.5,-.04]];
    const sides={triangle:3,pentagon:5,hexagon:6,hex:6,diamond:4}[shape];
    if(!sides)return null;
    const points=Array.from({length:sides},(_,i)=>{
      const angle=-Math.PI/2+2*Math.PI*i/sides;return [Math.cos(angle)*.5,Math.sin(angle)*.5];
    });
    const xs=points.map(p=>p[0]),ys=points.map(p=>p[1]),minX=Math.min(...xs),maxX=Math.max(...xs),minY=Math.min(...ys),maxY=Math.max(...ys);
    return points.map(([x,y])=>[(x-(minX+maxX)/2)/(maxX-minX),(y-(minY+maxY)/2)/(maxX-minX)]);
  }
  function drawNoteShape(ctx, rect, color, style, opacity=1) {
    const shape=style.note_shape||'rect',points=noteShapeVertices(shape),width=Math.max(2,rect[2]-rect[0]);
    const cx=(rect[0]+rect[2])/2,cy=(rect[1]+rect[3])/2;
    ctx.save();ctx.globalAlpha=clamp(opacity,0,1);ctx.fillStyle=color;ctx.beginPath();
    if(points){points.forEach(([x,y],i)=>i?ctx.lineTo(cx+x*width,cy+y*width):ctx.moveTo(cx+x*width,cy+y*width));ctx.closePath();}
    else if(shape==='circle')ctx.ellipse(cx,cy,Math.max(1,(width-2)/2),Math.max(1,(width-2)/2),0,0,Math.PI*2);
    else {
      const height=shape==='square'?width:Math.max(2,rect[3]-rect[1]);
      ctx.roundRect(cx-width/2,cy-height/2,width,height,clamp(Math.min(width,height)*.22,3,8));
    }
    ctx.fill();
    if(style.note_border!==false){
      ctx.strokeStyle=points?'#000000':blend(color,'#FFFFFF',.55);ctx.lineWidth=points?1.5:.85;
      ctx.globalAlpha=clamp(opacity*(style.note_outline_opacity??.78),0,1);ctx.stroke();
    }
    ctx.restore();
  }
  function paintText(ctx, content, rect, role, size, color, alpha=1, family='Segoe UI') {
    const largeHud=role==='combo'||role==='judgement',center=(rect[1]+rect[3])/2;
    const stroke=clamp(size/42,1,1.5);
    // Font edits keep their authored center and width. Allow taller glyphs and
    // the thin outline beyond the original rect instead of clipping large text.
    const halfHeight=largeHud?Math.max((rect[3]-rect[1])/2,size*.75):(rect[3]-rect[1])/2;
    ctx.save();ctx.fillStyle=color||'#F4F7FF';ctx.globalAlpha=clamp(alpha,0,1);
    ctx.font=`600 ${size}px "${String(family).replace(/["\\\n\r]/g,'')}", sans-serif`;
    ctx.textBaseline='middle';ctx.beginPath();
    ctx.rect(rect[0]-stroke,center-halfHeight-stroke,rect[2]-rect[0]+stroke*2,halfHeight*2+stroke*2);ctx.clip();
    const hex=/^#([a-f\d]{6})$/i.exec(color||''),rgb=hex?parseInt(hex[1],16):0xFFFFFF;
    const luminance=(.2126*(rgb>>16)+.7152*((rgb>>8)&255)+.0722*(rgb&255))/255;
    ctx.strokeStyle=luminance<.35?'#EAF3FD':'#061118';ctx.lineWidth=stroke;ctx.lineJoin='round';ctx.strokeText(content,rect[0],center,rect[2]-rect[0]);
    ctx.fillText(content,rect[0],center,rect[2]-rect[0]);ctx.restore();
  }
  const state={travel:[],last:0,mode:''};
  function paint(ctx, style, catalog, mode, now, animate, art, pressedLane=-1) {
    const native=settings(style,catalog),m=native.metrics,c=native.colors,motion=native.motion;
    const count=mode==='7+1'?8:parseInt(mode,10),left=470,right=1450,bottom=1080,width=(right-left)/count;
    const colors=style.lane_colors?.length?style.lane_colors:(catalog.palettes?.[mode]||['#F6F8FF','#4F80FF']);
    const line=clamp(style.judgement_line_position??.82,0,1)*bottom;
    const opacity=clamp(style.visual_opacity??1,.2,1),tick=now/1000,beat=animate?Math.floor(tick*2):0;
    const phase=animate?(tick*2)%1:0,hit=pressedLane>=0?pressedLane:beat%count;
    const dt=state.mode===mode?(now-state.last)/1000:1;state.mode=mode;state.last=now;
    const height=clamp(bottom-m.key_bottom_gap-line-12,Math.min(m.key_min_height,m.key_max_height),Math.max(m.key_min_height,m.key_max_height));
    const keyTop=bottom-m.key_bottom_gap-height;
    const box=(rect,color,alpha=1)=>{ctx.save();ctx.globalAlpha=clamp(alpha,0,1);ctx.fillStyle=color;ctx.fillRect(rect[0],rect[1],rect[2]-rect[0],rect[3]-rect[1]);ctx.restore();};
    const label=(key,content,rect,role=key,color=c[role],alpha=1)=>{
      rect=adjusted(rect,native.rects[key]);
      const size=m[role+'_font_size']||m.body_font_size;
      paintText(ctx,content,rect,role,size,color,alpha,native.fonts[role]||'Segoe UI');
    };
    const sprite=(slot,lane,rect,alpha=opacity)=>{
      const path=Array.isArray(style[slot])?style[slot][lane]:style[slot];
      if(path&&art(path.replace(/\{index:02\}/g,String(lane+1).padStart(2,'0')).replace(/\{index\}/g,String(lane+1)).replace(/\{lane\}/g,style.lane_map?.[lane]||String(lane+1)),rect,alpha))return;
      if(['note','hold_head','hold_tail'].includes(slot)&&(style.note_shape||'rect')!=='rect'&&!style.native?.sprites?.[slot]){
        drawNoteShape(ctx,rect,colors[lane%colors.length],style,alpha);return;
      }
      const meta=catalog.sprites[slot];
      drawSprite(ctx,native.sprites[slot],[meta.width,meta.height],rect,colors[lane%colors.length],alpha,style.note_border===false?0:clamp((style.note_outline_opacity??.78)/.78,0,1));
    };
    box([left,0,right,bottom],style.black_playfield===false?'#142231':'#000000');
    if(style.background)art(style.background,[0,0,1920,1080],style.background_opacity??.66);
    box([left,keyTop-2,right,bottom],c.chassis,opacity);
    ctx.save();ctx.beginPath();ctx.rect(left,0,right-left,bottom);ctx.clip();
    for(let lane=0;lane<count;lane++){
      const x=left+width*lane,color=colors[lane%colors.length];
      if(style.black_playfield===false)box([x,0,x+width,bottom],color,style.lane_background_opacity??.08);
      if(style.show_lane_dividers!==false)box([x,0,x+1,bottom],'#F6F8FF',m.lane_divider_opacity*.4);
      const inset=Math.min(m.key_inset,width*m.key_inset_ratio),isDown=lane===hit&&(pressedLane>=0||phase<.36);
      state.travel[lane]=animate||pressedLane>=0?advance(state.travel[lane]||0,isDown,dt,motion):0;
      const travel=state.travel[lane],depth=motion.press_depth*travel;
      const backdrop=backdropOpacity(style,isDown);
      if(backdrop>0&&backdropTop(style,0,bottom)<bottom)box([x+1,backdropTop(style,0,bottom),x+width-1,bottom],backdropColor(color,style),backdrop*opacity);
      const light=clamp((travel*motion.pressed_light+(lane===hit?Math.max(0,1-phase*3):0)*motion.hit_light)*(style.key_pulse_brightness??.7),0,1);
      box([x+inset,keyTop,x+width-inset,bottom-m.key_bottom_gap],c.key_well,opacity);
      const face=[x+inset,keyTop+depth,x+width-inset,Math.max(keyTop+1,bottom-m.key_bottom_gap-m.key_face_gap+depth)];
      sprite('key_idle',lane,face);if(light>0)sprite('key_pressed',lane,face,light*opacity);
      if(light>0)box([x+inset+5,bottom-5,x+inset+5+(width-2*inset-10)*light,bottom-3],blend(color,c.key_led,.35),opacity);
      const nwidth=Math.max(4,(width-12)*(style.note_width_ratio??1)),nh=32*(style.note_height_ratio??1);
      for(let n=0;n<2;n++){
        const y=90+((lane*127+n*337+(animate?tick*120:0))%Math.max(120,line-180));
        const rect=[x+(width-nwidth)/2,y,x+(width+nwidth)/2,y+nh];
        if(n===0&&lane%3===1){
          // As in the client, draw the body behind cap centers to bridge sloped
          // outlines and transparent sprite padding (including short holds).
          const tail=Math.max(0,y-210),rail=[rect[0],tail+nh*.325,rect[2],y+nh*.5];
          const bodyOpacity=clamp(style.hold_body_opacity??1,0,1)*opacity;
          if(!style.hold_body||!art(Array.isArray(style.hold_body)?style.hold_body[lane]:style.hold_body,rail,bodyOpacity)){
            const gradient=ctx.createLinearGradient(rail[0],0,rail[2],0);
            [[0,blend(color,c.hold_edge,m.hold_edge_mix)],[.17,blend(color,c.hold_shadow,.72)],[.5,blend(color,c.hold_core,m.hold_core_mix)],[.83,blend(color,c.hold_shadow,.72)],[1,blend(color,c.hold_edge,m.hold_edge_mix)]].forEach(([stop,value])=>gradient.addColorStop(stop,value));
            box(rail,gradient,bodyOpacity);
          }
          sprite('hold_head',lane,rect);if(style.show_hold_tail!==false)sprite('hold_tail',lane,[rect[0],tail,rect[2],tail+nh*.65]);
        }else sprite('note',lane,rect);
      }
      if(style.key_label_position!=='off')label('key_label',String(lane+1),[x+6,style.key_label_position==='top'?20:bottom-28,x+width-4,style.key_label_position==='top'?44:bottom-4]);
    }
    if(style.show_judgement_line!==false){
      if(style.judgement_line_glow!==false)box([left,line-m.judgement_glow_height/2,right,line+m.judgement_glow_height/2],c.judgement_glow,.1*opacity);
      const thickness=judgementLineWidth(m.judgement_line_width,style.note_height_ratio??1);
      box([left,line-thickness/2,right,line+thickness/2],c.judgement_line,opacity);
    }
    if(animate&&phase<.55){const x=left+(hit+.5)*width,life=1-phase/.55;
      box([x-width*.35,line-motion.burst_height*life,x+width*.35,line+3],colors[hit%colors.length],life*.28);
      box([x-1,line-motion.burst_rise*(1-life),x+1,line],c.burst_trail,life);
      box([x-width*.38,line-2,x+width*.38,line+1],c.burst_core,life);
    }
    ctx.restore();
    label('title','LUMA KEYS / '+mode.toUpperCase(),[84,48,458,92]);
    label('artist','TenRiff · Digital instrument',[84,95,458,127],'body');
    label('speed','RATE ×1.00 / HS 10.0',[84,130,458,159],'body');
    label('score','SCORE  950,000',[1540,48,1870,96]);
    label('stats','PG 260  GR 12  BAD 0',[1510,105,1870,140],'body');
    const progress=adjusted([84,16,454,36],native.rects.progress);box(progress,'#1D2735');box([progress[0],progress[1],progress[0]+(progress[2]-progress[0])*.38,progress[3]],c.combo,.75);
    const gauge=adjusted([1512,210,1554,906],native.rects.gauge);box(gauge,'#142332');box([gauge[0]+4,gauge[1]+(gauge[3]-gauge[1])*.25,gauge[2]-4,gauge[3]-4],c.gauge_normal);
    const grades=['pg','gr','gd','bd','pr'],grade=animate?Math.floor(tick/3)%grades.length:0;
    const judgementAlpha=animate?clamp(1-phase*500/motion.judgement_duration_ms,0,1):1;
    label('judgement',['P GREAT','GREAT','GOOD','BAD','POOR'][grade],[858,192,1150,260],'judgement',c['judgement_'+grades[grade]],judgementAlpha);
    const life=animate?Math.max(0,1-phase*500/motion.combo_duration_ms):0,scale=1+(motion.combo_scale-1)*life;
    ctx.save();ctx.translate(960,324);ctx.scale(scale,scale);ctx.translate(-960,-324);
    label('combo','147',[920,290+motion.combo_lift*life,1080,348+motion.combo_lift*life]);ctx.restore();
    // Independent switches retain legacy single-switch behavior when bar is absent.
    const errorMs=grade%2?-28:24;
    if(grade!==0&&judgementAlpha>0){
      if(style.show_timing_feedback!==false) label('timing_label',(errorMs<0?'FAST ':'SLOW ')+Math.abs(errorMs)+' ms',[858,258,1150,280],'body',errorMs<0?c.timing_fast:c.timing_slow,judgementAlpha);
      if((style.show_timing_bar??style.show_timing_feedback)!==false){
      const timing=adjusted([960-m.timing_half_width,275,960+m.timing_half_width,275+m.timing_height],native.rects.timing);
      const center=(timing[0]+timing[2])/2,half=(timing[2]-timing[0])/2;
      box(timing,c.timing,.25);box([center-1,timing[1]-4,center+1,timing[3]+4],c.timing);
      for(const ms of [-27,-12,8,19]){const x=center+clamp(ms/m.timing_range_ms,-1,1)*half;
        box([x-1,timing[1]-2,x+1,timing[3]+2],ms<0?c.timing_fast:c.timing_slow,.8);
      }
    }
    }
    if(style.gear)art(style.gear,[left,0,right,bottom],opacity);
  }
  function judgementLineWidth(width, ratio=1) {
    return width*(Number.isFinite(ratio)?clamp(ratio,.5/1.8,4/1.8):1);
  }
  function backdropOpacity(style, pressed) {
    const opacity=style.key_backdrop_opacity??.25;
    return style.key_backdrop!==false&&pressed&&Number.isFinite(opacity)?clamp(opacity,0,1):0;
  }
  function backdropColor(color, style) {
    const value=style.key_backdrop_brightness??1,gain=Number.isFinite(value)?clamp(value,0,2):1;
    const match=/^#([0-9a-f]{6})([0-9a-f]{2})?$/i.exec(color);
    if(!match)return color;
    const rgb=parseInt(match[1],16),bytes=[16,8,0].map(shift=>Math.round(Math.min(255,((rgb>>shift)&255)*gain)).toString(16).padStart(2,'0'));
    return '#'+bytes.join('').toUpperCase()+(match[2]||'');
  }
  function backdropTop(style, top, bottom) {
    const height=style.key_backdrop_height??1,ratio=Number.isFinite(height)?clamp(height,0,1):1;
    return bottom-Math.max(0,bottom-top)*ratio;
  }
  return {groups,settings,blend,advance,adjusted,drawSprite,noteShapeVertices,drawNoteShape,paint,paintText,judgementLineWidth,backdropOpacity,backdropColor,backdropTop};
});
