/* Dependency-free ZIP (stored entries). UTF-8 names; no original files are written. */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.TenRiffZip = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; ++n) {
    let c = n;
    for (let k = 0; k < 8; ++k) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    table[n] = c;
  }
  function crc32(data) {
    let crc = 0xffffffff;
    for (const byte of data) crc = table[(crc ^ byte) & 255] ^ (crc >>> 8);
    return (crc ^ 0xffffffff) >>> 0;
  }
  function safeName(name) {
    return typeof name === 'string' && name && !name.startsWith('/') && !/[\\\x00-\x1f:*?"<>|]/.test(name) &&
      !name.split('/').some(part => !part || part === '..' || part === '.');
  }
  function create(entries) {
    if (entries.length > 65535) throw new Error('Too many ZIP entries');
    const encoder = new TextEncoder(), parts = [], central = [], names = new Set();
    let offset = 0, centralSize = 0;
    for (const entry of entries) {
      if (!safeName(entry.name) || names.has(entry.name.toLowerCase())) throw new Error('Unsafe or duplicate ZIP entry');
      names.add(entry.name.toLowerCase());
      const name = encoder.encode(entry.name), data = entry.data;
      if (!(data instanceof Uint8Array) || name.length > 65535 || offset + data.length > 536870912) throw new Error('ZIP exceeds 512 MB');
      const crc = crc32(data), local = new Uint8Array(30 + name.length), lv = new DataView(local.buffer);
      lv.setUint32(0, 0x04034b50, true); lv.setUint16(4, 20, true); lv.setUint16(6, 0x0800, true);
      lv.setUint16(12, 33, true); // 1980-01-01, reproducible metadata.
      lv.setUint32(14, crc, true); lv.setUint32(18, data.length, true); lv.setUint32(22, data.length, true); lv.setUint16(26, name.length, true);
      local.set(name, 30); parts.push(local, data);
      const record = new Uint8Array(46 + name.length), cv = new DataView(record.buffer);
      cv.setUint32(0, 0x02014b50, true); cv.setUint16(4, 20, true); cv.setUint16(6, 20, true); cv.setUint16(8, 0x0800, true); cv.setUint16(14, 33, true);
      cv.setUint32(16, crc, true); cv.setUint32(20, data.length, true); cv.setUint32(24, data.length, true); cv.setUint16(28, name.length, true); cv.setUint32(42, offset, true);
      record.set(name, 46); central.push(record); centralSize += record.length; offset += local.length + data.length;
    }
    const end = new Uint8Array(22), ev = new DataView(end.buffer);
    ev.setUint32(0, 0x06054b50, true); ev.setUint16(8, entries.length, true); ev.setUint16(10, entries.length, true); ev.setUint32(12, centralSize, true); ev.setUint32(16, offset, true);
    return [...parts, ...central, end];
  }
  return {crc32, safeName, create};
});
