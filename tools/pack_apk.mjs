#!/usr/bin/env node
/**
 * Repackages the APK without zipalign: swaps in the freshly built libnexora.so and the current
 * asset tree, stores+aligns native libs the same way apk_sign_ts's internal aligner expects,
 * so signing never has to shift the file offsets afterwards.
 *
 *   node tools/pack_apk.mjs <base.apk> <out-unsigned.apk> <libnexora.so> [assetsDir]
 */
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { createRequire } from 'node:module';

const [base, out, soPath, assetsDir] = process.argv.slice(2);
if (!base || !out || !soPath) {
  console.error('usage: pack_apk.mjs <base.apk> <out.apk> <libnexora.so> [assets]');
  process.exit(2);
}

const require = createRequire(import.meta.url);
const zlib = require('zlib');
// The published package does not export its zip helpers; import the module by file path,
// which bypasses the package "exports" map.
const pkgMain = require.resolve('apk_sign_ts');
const Z = await import(path.join(path.dirname(pkgMain), 'ZipUtils.js'));

const LIB_ALIGN = 4096;   // matches the aligner used inside apk_sign_ts

const work = fs.mkdtempSync(path.join(os.tmpdir(), 'nexora-pack-'));
const tree = path.join(work, 'tree');
fs.mkdirSync(path.join(tree, 'lib', 'arm64-v8a'), { recursive: true });
fs.copyFileSync(soPath, path.join(tree, 'lib', 'arm64-v8a', 'libnexora.so'));
if (assetsDir) execFileSync('cp', ['-a', path.resolve(assetsDir), path.join(tree, 'assets')]);

const src = new Uint8Array(fs.readFileSync(base));
const cd = Z.parseZipCentralDirectory(src);
const eocd = Z.findEndOfCentralDirectory(src);
const entries = [...cd.entries].sort((a, b) => a.localHeaderOffset - b.localHeaderOffset);

// Collect every file that goes into the new archive.
const files = [];
const seen = new Set();
const addTree = (dir, prefix) => {
  for (const name of fs.readdirSync(dir).sort()) {
    const full = path.join(dir, name);
    const stat = fs.statSync(full);
    if (stat.isDirectory()) addTree(full, prefix + name + '/');
    else {
      files.push({ name: prefix + name, data: new Uint8Array(fs.readFileSync(full)) });
      seen.add(prefix + name);
    }
  }
};
addTree(tree, '');
for (const e of entries) {
  if (seen.has(e.name) || e.name.startsWith('META-INF/')) continue;
  // Anything under assets/ or lib/ comes from the tree above (that is the point of a repack).
  if (e.name.startsWith('assets/') || e.name.startsWith('lib/')) continue;
  const off = Z.getEntryDataOffset(src, e.localHeaderOffset);
  files.push({ name: e.name, data: src.subarray(off, off + e.compressedSize),
               method: e.compressionMethod, fromBase: e.compressionMethod === 8 });
}

// CRC of the *uncompressed* bytes. `content` is whatever is written to the archive.
function crcOfEntry(content, method) {
  const plain = method === 0 ? Buffer.from(content) : zlib.inflateRawSync(Buffer.from(content));
  const crc = zlib.crc32(plain);
  return typeof crc === 'number' ? crc >>> 0 : Z.computeCrc32(new Uint8Array(plain));
}

function localHeader(name, crc, csize, usize, method, extra) {
  const h = new Uint8Array(30 + name.length + extra.length);
  const v = new DataView(h.buffer);
  v.setUint32(0, 0x04034b50, true);
  v.setUint16(4, 20, true);          // version needed
  v.setUint16(6, 0, true);           // flags
  v.setUint16(8, method, true);
  v.setUint16(10, 0, true);          // mod time
  v.setUint16(12, 0x21, true);       // mod date (1980-01-01)
  v.setUint32(14, crc, true);
  v.setUint32(18, csize, true);
  v.setUint32(22, usize, true);
  v.setUint16(26, name.length, true);
  v.setUint16(28, extra.length, true);
  h.set(name, 30);
  h.set(extra, 30 + name.length);
  return h;
}

function centralHeader(name, crc, csize, usize, localOffset, method, extra) {
  const h = new Uint8Array(46 + name.length + extra.length);
  const v = new DataView(h.buffer);
  v.setUint32(0, 0x02014b50, true);
  v.setUint16(4, 20, true);          // version made by
  v.setUint16(6, 20, true);          // version needed
  v.setUint16(8, 0, true);
  v.setUint16(10, method, true);
  v.setUint16(12, 0, true);
  v.setUint16(14, 0x21, true);
  v.setUint32(16, crc, true);
  v.setUint32(20, csize, true);
  v.setUint32(24, usize, true);
  v.setUint16(28, name.length, true);
  v.setUint16(30, extra.length, true);
  v.setUint16(32, 0, true);          // comment length
  v.setUint16(34, 0, true);          // disk
  v.setUint16(36, 0, true);          // internal attrs
  v.setUint32(38, 0, true);          // external attrs
  v.setUint32(42, localOffset, true);
  h.set(name, 46);
  h.set(extra, 46 + name.length);
  return h;
}

const locals = [];
const headers = [];
let localOffset = 0;
for (const f of files) {
  const nameBytes = new TextEncoder().encode(f.name);
  const storeAndAlign = f.name === 'resources.arsc' || (f.name.startsWith('lib/') && f.name.endsWith('.so'));
  let method = f.method ?? 0;
  const fromBase = method === 8 && f.data !== undefined && f.fromBase === true;
  let content = f.data;
  if (!storeAndAlign && method === 8) {
    // already deflated in the source archive: keep the bytes as-is
  } else if (!storeAndAlign) {
    method = 8;
    content = fromBase ? f.data : zlib.deflateRawSync(Buffer.from(f.data), { level: 9 });
  } else {
    method = 0;
  }
  // CRC must describe the *uncompressed* bytes; for entries copied from the base archive we
  // only have the deflated stream here, so recompute from the inflated content below.
  const crc = crcOfEntry(content, method);
  const usize = method === 0 ? content.length : zlib.inflateRawSync(Buffer.from(content)).length;
  let extra = new Uint8Array(0);
  if (storeAndAlign) {
    const align = f.name === 'resources.arsc' ? 4 : LIB_ALIGN;
    const pad = (align - ((localOffset + 30 + nameBytes.length) % align)) % align;
    extra = new Uint8Array(pad);
  }
  // The library's helpers write `data.length` into the size fields, which is wrong for
  // compressed entries (they would report the compressed size as the uncompressed one) and
  // makes the resulting archive unreadable. Build the headers here with both sizes explicit.
  const header = localHeader(nameBytes, crc, content.length, usize, method, extra);
  const cdHeader = centralHeader(nameBytes, crc, content.length, usize, localOffset, method, extra);
  locals.push(header, content);
  headers.push(cdHeader);
  localOffset += header.length + content.length;
}
const localBytes = Buffer.concat(locals.map((b) => Buffer.from(b.buffer ? b : b)));
const cdBytes = Buffer.concat(headers.map((h) => Buffer.from(h)));
const eocdRecord = new Uint8Array(22);
const dv = new DataView(eocdRecord.buffer);
dv.setUint32(0, 0x06054b50, true);
dv.setUint16(8, headers.length, true);
dv.setUint16(10, headers.length, true);
dv.setUint32(12, cdBytes.length, true);
dv.setUint32(16, localBytes.length, true);
fs.writeFileSync(out, Buffer.concat([localBytes, cdBytes, Buffer.from(eocdRecord)]));
fs.rmSync(work, { recursive: true, force: true });
console.log(`packed ${out}: ${(fs.statSync(out).size / 1048576).toFixed(2)} MB, ${headers.length} entries`);
