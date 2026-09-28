/* Shared document logic: browser globals for file://, CommonJS for Node tests. */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.TenRiffSkin = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  const own = (object, key) => object != null && Object.prototype.hasOwnProperty.call(object, key);
  const object = value => value !== null && typeof value === 'object' && !Array.isArray(value);
  const clone = value => value === undefined ? undefined : JSON.parse(JSON.stringify(value));
  const serialize = value => JSON.stringify(value, null, 2) + '\n';
  const get = (value, path) => path.reduce((node, key) => own(node, key) ? node[key] : undefined, value);
  // Native and legacy title screens share slot names, but have different base geometry.
  const nativeTitleLayout = {
    logo: [56,24,418,94], buttons: [1016,260,1824,824], guide: [96,516,932,924],
    footer: [64,972,1856,1048], spectrum: [1430,990,1574,1028]
  };

  function put(target, key, value) {
    // JSON may contain __proto__; keep it as inert data, never as a setter.
    Object.defineProperty(target, key, {value, enumerable: true, writable: true, configurable: true});
  }
  function set(document, path, value) {
    if (!path.length) return clone(value);
    const next = clone(document);
    let node = next;
    path.slice(0, -1).forEach(key => {
      if (!own(node, key) || !object(node[key])) put(node, key, {});
      node = node[key];
    });
    if (value === undefined) delete node[path[path.length - 1]];
    else put(node, path[path.length - 1], clone(value));
    return next;
  }
  function parse(text) {
    const value = JSON.parse(text.replace(/^\uFEFF/, ''));
    if (!object(value)) throw new Error('rootObject');
    return value;
  }
  function resolve(schema, rootSchema) {
    if (!schema || !schema.$ref) return schema || {};
    const path = schema.$ref.replace(/^#\//, '').split('/').map(k => k.replace(/~1/g, '/').replace(/~0/g, '~'));
    return get(rootSchema, path) || {};
  }
  function safeAsset(path) {
    if (typeof path !== 'string') return false;
    if (!path) return true;
    const normalized = path.replace(/\\/g, '/').replace(/\{index:02\}/g, '01');
    return !normalized.startsWith('/') && !/[\x00-\x1f:*?"<>|]/.test(normalized) &&
      !normalized.split('/').some(p => p === '..') && /\.(png|jpe?g|bmp)$/i.test(normalized);
  }
  function normalizeAsset(path) {
    return String(path).replace(/\\/g, '/').split('/').filter(part => part && part !== '.').join('/');
  }
  function validate(document, rootSchema) {
    const issues = [];
    const add = (path, code, detail, severity = 'error') => issues.push({path: path.join('.'), code, detail, severity});
    function visit(value, sourceSchema, path) {
      const ref = sourceSchema && sourceSchema.$ref;
      const schema = resolve(sourceSchema, rootSchema);
      if (schema.oneOf) {
        const choices = schema.oneOf.map(s => resolve(s, rootSchema));
        const branch = choices.find(s => s.type === (Array.isArray(value) ? 'array' : typeof value));
        if (!branch) { add(path, 'type', 'string | array'); return; }
        if (typeof value === 'string' && ref === '#/$defs/laneAssets' && !safeAsset(value)) add(path, 'assetPath');
        visit(value, branch, path);
        return;
      }
      if (schema.const !== undefined && value !== schema.const) add(path, 'constant', String(schema.const));
      if (schema.enum && !schema.enum.includes(value)) add(path, 'enum', schema.enum.join(', '));
      const actual = Array.isArray(value) ? 'array' : value === null ? 'null' : typeof value;
      if (schema.type && !(schema.type === 'integer' ? Number.isInteger(value) : actual === schema.type)) {
        add(path, 'type', schema.type); return;
      }
      if (typeof value === 'number') {
        if (!Number.isFinite(value)) add(path, 'finite');
        if (schema.minimum !== undefined && value < schema.minimum) add(path, 'minimum', schema.minimum);
        if (schema.maximum !== undefined && value > schema.maximum) add(path, 'maximum', schema.maximum);
        if (schema.exclusiveMinimum !== undefined && value <= schema.exclusiveMinimum) add(path, 'exclusiveMinimum', schema.exclusiveMinimum);
      }
      if (typeof value === 'string') {
        if (schema.minLength !== undefined && value.length < schema.minLength) add(path, 'minLength', schema.minLength);
        if (schema.maxLength !== undefined && value.length > schema.maxLength) add(path, 'maxLength', schema.maxLength);
        if (schema.pattern && !new RegExp(schema.pattern).test(value)) add(path, 'pattern', schema.pattern);
        if (ref === '#/$defs/assetPath' && !safeAsset(value)) add(path, 'assetPath');
        if (path[0] === 'native' && path[1] === 'fonts' && new TextEncoder().encode(value).length > 128) add(path, 'fontBytes', 128);
      }
      if (Array.isArray(value)) {
        if (schema.minItems !== undefined && value.length < schema.minItems) add(path, 'minItems', schema.minItems);
        if (schema.maxItems !== undefined && value.length > schema.maxItems) add(path, 'maxItems', schema.maxItems);
        value.forEach((item, index) => schema.items && visit(item, schema.items, [...path, String(index)]));
        if (ref === '#/$defs/layoutRect' && value.length === 4 && !(value[2] > value[0] && value[3] > value[1])) add(path, 'rectangle');
      }
      if (object(value)) {
        if (schema.maxProperties !== undefined && Object.keys(value).length > schema.maxProperties) add(path, 'maxItems', schema.maxProperties);
        (schema.required || []).forEach(key => { if (!own(value, key)) add([...path, key], 'required'); });
        Object.keys(value).forEach(key => {
          if (schema.propertyNames) {
            const rule = resolve(schema.propertyNames, rootSchema);
            if ((rule.enum && !rule.enum.includes(key)) || (rule.pattern && !new RegExp(rule.pattern).test(key))) add([...path, key], 'pattern', rule.pattern || rule.enum.join(', '));
          }
          let child = own(schema.properties, key) ? schema.properties[key] : undefined;
          if (!child) Object.entries(schema.patternProperties || {}).some(([pattern, item]) => {
            if (!new RegExp(pattern).test(key)) return false;
            child = item; return true;
          });
          if (!child && object(schema.additionalProperties)) child = schema.additionalProperties;
          if (child) visit(value[key], child, [...path, key]);
          else if (schema.additionalProperties === false) add([...path, key], 'unknown', null, 'warning');
        });
      }
    }
    visit(document, rootSchema, []);
    return issues;
  }
  function createHistory(initial, limit = 100) {
    let current = clone(initial), past = [], future = [], saved = serialize(initial);
    return {
      get document() { return clone(current); },
      get canUndo() { return past.length > 0; },
      get canRedo() { return future.length > 0; },
      get dirty() { return saved !== serialize(current); },
      commit(next) {
        if (serialize(next) === serialize(current)) return false;
        past.push(current); if (past.length > limit) past.shift();
        current = clone(next); future = []; return true;
      },
      load(next) { current = clone(next); past = []; future = []; saved = serialize(next); },
      markSaved(value = current) { saved = serialize(value); },
      undo() { if (!past.length) return false; future.push(current); current = past.pop(); return true; },
      redo() { if (!future.length) return false; past.push(current); current = future.pop(); return true; }
    };
  }
  function gameplay(document, mode) {
    const common = clone(document.gameplay || {});
    const override = clone(get(common, ['modes', mode]) || {});
    delete common.modes;
    const result = {...common, ...override};
    // Only native categories merge by slot. Asset arrays and explicit empty sprite
    // arrays still replace the common value, matching the client manifest loader.
    if (object(common.native) || object(override.native)) {
      result.native = {...(common.native || {}), ...(override.native || {})};
      for (const group of ['metrics', 'colors', 'motion', 'rects', 'fonts', 'sprites']) {
        if (object(common.native?.[group]) || object(override.native?.[group]))
          result.native[group] = {...(common.native?.[group] || {}), ...(override.native?.[group] || {})};
      }
    }
    return result;
  }
  function laneAsset(value, index, laneMap) {
    const path = Array.isArray(value) ? value[index] || '' : value || '';
    return String(path).replace(/\{index:02\}/g, String(index + 1).padStart(2, '0'))
      .replace(/\{index\}/g, String(index + 1)).replace(/\{lane\}/g, (laneMap || [])[index] || String(index + 1));
  }
  function assetReferences(document) {
    const result = new Set();
    const add = value => { if (typeof value === 'string' && value) result.add(normalizeAsset(value)); };
    const lobby = document.lobby || {};
    add(lobby.background); add(lobby.logo);
    Object.values(lobby.screen_backgrounds || {}).forEach(add);
    Object.values(get(document, ['native', 'assets']) || {}).forEach(add);
    const slots = ['background', 'gear', 'note', 'hold_head', 'hold_body', 'hold_tail', 'key_idle', 'key_pressed'];
    for (let count = 1; count <= 17; ++count) {
      const mode = count === 17 ? '7+1' : `${count}k`, lanes = count === 17 ? 8 : count;
      const style = gameplay(document, mode);
      slots.forEach(slot => {
        const value = style[slot];
        if (!value) return;
        if (slot === 'background' || slot === 'gear') add(value);
        else for (let i = 0; i < lanes; ++i) add(laneAsset(value, i, style.lane_map));
      });
    }
    return [...result];
  }
  function moveRect(rect, dx, dy, resize, snap = 1) {
    const round = number => Math.round(number / snap) * snap;
    if (resize) return [rect[0], rect[1], Math.min(8192, Math.max(rect[0] + 8, round(rect[2] + dx))), Math.min(8192, Math.max(rect[1] + 8, round(rect[3] + dy)))];
    dx = Math.max(-8192 - Math.min(rect[0], rect[2]), Math.min(8192 - Math.max(rect[0], rect[2]), round(dx)));
    dy = Math.max(-8192 - Math.min(rect[1], rect[3]), Math.min(8192 - Math.max(rect[1], rect[3]), round(dy)));
    return [rect[0] + dx, rect[1] + dy, rect[2] + dx, rect[3] + dy];
  }
  return {own, object, clone, serialize, get, set, parse, resolve, safeAsset, normalizeAsset, validate, createHistory, gameplay, laneAsset, assetReferences, moveRect, nativeTitleLayout};
});
