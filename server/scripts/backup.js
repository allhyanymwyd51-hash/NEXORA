#!/usr/bin/env node
// NEXORA — consistent SQLite backup with rotation.
//
//   node scripts/backup.js [--dir data/backups] [--keep 14]
//
// Uses SQLite's own BACKUP-safe approach: the database is opened through the same
// `node:sqlite` driver the server uses, a checkpoint is forced so the WAL is folded in,
// and the resulting file is written atomically (tmp + rename) so a crash mid-copy can
// never leave a truncated backup in place. Rotates old copies beyond `--keep`.
import fs from 'node:fs';
import path from 'node:path';
import { config, ROOT } from '../src/core/config.js';

function arg(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : fallback;
}

const outDir = path.resolve(ROOT, arg('dir', config.backupDir));
const keep = Math.max(1, Number(arg('keep', 14)) || 14);

function stamp(d = new Date()) {
  const p = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

async function main() {
  if (!fs.existsSync(config.dbPath)) {
    console.error(`[backup] database not found: ${config.dbPath}`);
    process.exit(1);
  }
  fs.mkdirSync(outDir, { recursive: true });

  const { DatabaseSync } = await import('node:sqlite');
  const db = new DatabaseSync(config.dbPath);
  db.exec('PRAGMA busy_timeout = 5000;');
  // Fold the WAL into the main file so a plain file copy is complete and self-consistent.
  db.exec('PRAGMA wal_checkpoint(TRUNCATE);');
  const integrity = db.prepare('PRAGMA integrity_check;').get();
  const ok = Object.values(integrity || {})[0] === 'ok';
  db.close();
  if (!ok) {
    console.error('[backup] integrity_check failed, refusing to write a bad backup');
    process.exit(2);
  }

  const target = path.join(outDir, `nexora-${stamp()}.db`);
  const tmp = `${target}.tmp`;
  fs.copyFileSync(config.dbPath, tmp);
  fs.renameSync(tmp, target);

  // rotate
  const files = fs
    .readdirSync(outDir)
    .filter((f) => /^nexora-\d{8}-\d{6}\.db$/.test(f))
    .sort();
  for (const f of files.slice(0, Math.max(0, files.length - keep))) {
    try { fs.unlinkSync(path.join(outDir, f)); } catch { /* best effort */ }
  }

  const size = fs.statSync(target).size;
  console.log(`[backup] ok ${target} (${(size / 1024 / 1024).toFixed(2)} MB), kept ${Math.min(files.length, keep)}`);
}

main().catch((e) => {
  console.error('[backup] failed:', e.message);
  process.exit(1);
});
