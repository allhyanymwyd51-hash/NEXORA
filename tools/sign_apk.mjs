#!/usr/bin/env node
/**
 * Signs an APK v1+v2+v3 without a JDK (apk_sign_ts instead of apksigner).
 *
 *   node tools/sign_apk.mjs <in.apk> <out.apk> <key.pem> <cert.pem>
 *
 * The library ships a zipalign pass that pads native libraries to 4096 bytes. Android 15 on
 * 16 KB page devices requires the uncompressed .so to start on a 16 KB boundary, so the
 * aligner is re-created in place (same package directory, so its imports still resolve) with
 * the page size raised to 16384.
 */
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';

const PAGE = 16384;

const [inApk, outApk, keyPem, certPem] = process.argv.slice(2);
if (!inApk || !outApk || !keyPem || !certPem) {
  console.error('usage: sign_apk.mjs <in.apk> <out.apk> <key.pem> <cert.pem>');
  process.exit(2);
}

const require = createRequire(import.meta.url);
// resolve('apk_sign_ts') returns dist/index.js, so its directory is the dist directory
const distDir = path.dirname(require.resolve('apk_sign_ts'));
const pkgDir = path.dirname(distDir);
const alignedDir = path.join(pkgDir, `dist-p${PAGE}`);

function makeAlignedCopy() {
  fs.rmSync(alignedDir, { recursive: true, force: true });
  fs.cpSync(distDir, alignedDir, { recursive: true });
  const zipUtils = path.join(alignedDir, 'ZipUtils.js');
  const src = fs.readFileSync(zipUtils, 'utf8')
      .replace(/entry\.name === 'resources\.arsc' \? 4 : 4096/, `entry.name === 'resources.arsc' ? 4 : ${PAGE}`);
  if (!src.includes(`: ${PAGE}`)) throw new Error('could not raise the zip alignment to ' + PAGE);
  fs.writeFileSync(zipUtils, src);
}

makeAlignedCopy();
const { ApkSigner, SigningKey } = await import(pathToFileURL(path.join(alignedDir, 'index.js')).href);

const key = SigningKey.fromPEM(fs.readFileSync(keyPem, 'utf8'), fs.readFileSync(certPem, 'utf8'));
const signer = new ApkSigner({ signingKey: key });
const { signedApk } = await signer.sign(new Uint8Array(fs.readFileSync(inApk)));
fs.writeFileSync(outApk, Buffer.from(signedApk));
fs.rmSync(alignedDir, { recursive: true, force: true });
console.log(`signed ${outApk} (${(fs.statSync(outApk).size / 1048576).toFixed(2)} MB, ${PAGE}-byte lib alignment)`);
