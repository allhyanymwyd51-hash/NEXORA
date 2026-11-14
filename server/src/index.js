// NEXORA backend entry point.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { config, ROOT } from './core/config.js';
import { openDb, all, get, run } from './core/db.js';
import { attachRealtime, bindDb, pushNotification } from './core/realtime.js';
import { handle } from './api/router.js';
import { notify } from './core/auth.js';
import './api/auth.routes.js';
import './api/maps.routes.js';
import './api/social.routes.js';
import './api/shop.routes.js';
import './api/ai.routes.js';
import './api/admin.routes.js';

const db = openDb();
bindDb(all);

// --- tiny static file server for the web dashboard (not the game client) ---
const PUBLIC_DIR = path.join(ROOT, 'public');
const MIME = {
  '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg',
  '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.webp': 'image/webp', '.txt': 'text/plain; charset=utf-8',
};

function serveStatic(req, res, pathname) {
  let rel = pathname === '/' ? '/index.html' : pathname;
  if (rel.includes('..')) { res.writeHead(403); return res.end('forbidden'); }
  const file = path.join(PUBLIC_DIR, rel);
  if (!file.startsWith(PUBLIC_DIR) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
    res.writeHead(404, { 'content-type': 'text/plain' });
    return res.end('not found');
  }
  const ext = path.extname(file).toLowerCase();
  res.writeHead(200, { 'content-type': MIME[ext] || 'application/octet-stream', 'cache-control': 'no-cache' });
  fs.createReadStream(file).pipe(res);
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  if (url.pathname.startsWith('/api/') || url.pathname === '/health') {
    if (url.pathname === '/health') {
      res.writeHead(200, { 'content-type': 'application/json' });
      return res.end(JSON.stringify({ ok: true, uptime: Math.round(process.uptime()), version: config.version }));
    }
    return handle(req, res);
  }
  // WebSocket endpoint is upgraded before reaching here.
  serveStatic(req, res, url.pathname);
});

attachRealtime(server);

server.listen(config.port, config.host, () => {
  console.log(`[NEXORA] backend listening on http://${config.host}:${config.port}`);
  console.log(`[NEXORA] database: ${config.dbPath}`);
  if (!get('SELECT id FROM users WHERE role = ?', 'owner')) {
    console.log('[NEXORA] ⚠ no owner account yet — run: npm run owner');
  }
});

// daily maintenance: expire sessions, prune old notifications
const maintenance = setInterval(() => {
  try {
    run('DELETE FROM sessions WHERE expires_at <= ?', Math.floor(Date.now() / 1000));
    run('DELETE FROM notifications WHERE created_at < ?', Math.floor(Date.now() / 1000) - 30 * 86400);
  } catch (e) { console.error('[NEXORA] maintenance', e.message); }
}, 3600000);
if (maintenance.unref) maintenance.unref();

function shutdown() {
  console.log('[NEXORA] shutting down');
  server.close(() => process.exit(0));
  setTimeout(() => process.exit(0), 2000);
}
process.on('SIGTERM', shutdown);
process.on('SIGINT', shutdown);
