// Sessions, auth guards, audit trail and notifications.
import { all, get, run } from './db.js';
import { config } from './config.js';
import { newId, randomToken, sha256 } from './security.js';
import { nowSec, unauthorized, forbidden } from './util.js';
import { getUserById, rankOf } from './users.js';

export function createSession(userId, device = '') {
  const token = randomToken(32);
  const id = newId('sess');
  const now = nowSec();
  run(
    'INSERT INTO sessions(id,user_id,token_hash,created_at,expires_at,device) VALUES(?,?,?,?,?,?)',
    id, userId, sha256(token), now, now + config.sessionDays * 86400, String(device).slice(0, 200)
  );
  return { token, id, expiresAt: now + config.sessionDays * 86400 };
}

export function userFromToken(token) {
  if (!token) return null;
  const row = get(
    `SELECT s.*, u.* FROM sessions s JOIN users u ON u.id = s.user_id WHERE s.token_hash = ? AND s.expires_at > ?`,
    sha256(token), nowSec()
  );
  if (!row) return null;
  if (row.banned) return { banned: true, id: row.user_id, username: row.username, role: row.role, ban_reason: row.ban_reason };
  return row;
}

export function destroySession(token) {
  if (!token) return;
  run('DELETE FROM sessions WHERE token_hash = ?', sha256(token));
}

export function destroyUserSessions(userId) {
  run('DELETE FROM sessions WHERE user_id = ?', userId);
}

export function bearer(req) {
  const h = req.headers.authorization || '';
  if (h.startsWith('Bearer ')) return h.slice(7).trim();
  const q = new URL(req.url, 'http://x').searchParams.get('token');
  return q || '';
}

// Router-level guards -------------------------------------------------------
export function guard(ctx, opts) {
  if (!opts.auth) return null;
  const u = ctx.user;
  if (!u) throw unauthorized();
  if (u.banned) throw forbidden('الحساب محظور');
  if (opts.role) {
    const need = rankOf({ role: opts.role });
    if (rankOf(u) < need) throw forbidden();
  }
  if (opts.owner && u.role !== 'owner') throw forbidden('هذه الصفحة للمالك فقط');
  return u;
}

// Audit ---------------------------------------------------------------------
export function audit(actorId, action, target = '', detail = {}, ip = '') {
  run(
    'INSERT INTO audit_log(id,actor_id,action,target,detail,ip,created_at) VALUES(?,?,?,?,?,?,?)',
    newId('aud'), actorId || '', action, target, JSON.stringify(detail), ip, nowSec()
  );
}

// Notifications --------------------------------------------------------------
export function notify(userId, type, title, body = '', data = {}) {
  if (!userId) return null;
  const id = newId('ntf');
  run(
    'INSERT INTO notifications(id,user_id,type,title,body,data,created_at) VALUES(?,?,?,?,?,?,?)',
    id, userId, type, title, body, JSON.stringify(data), nowSec()
  );
  // Fire and forget into the realtime hub if it is loaded (avoids circular import).
  import('./realtime.js').then((m) => m.pushNotification(userId, {
    id, type, title, body, data, createdAt: nowSec(),
  })).catch(() => {});
  return id;
}

export function notificationsFor(userId, limit = 40) {
  return all(
    'SELECT * FROM notifications WHERE user_id = ? ORDER BY created_at DESC LIMIT ?', userId, limit
  ).map((n) => ({
    id: n.id, type: n.type, title: n.title, body: n.body,
    data: JSON.parse(n.data || '{}'), read: n.read_at > 0, createdAt: n.created_at,
  }));
}

export function markNotificationsRead(userId, ids) {
  if (!Array.isArray(ids) || !ids.length) {
    run('UPDATE notifications SET read_at = ? WHERE user_id = ? AND read_at = 0', nowSec(), userId);
    return;
  }
  const stmt = 'UPDATE notifications SET read_at = ? WHERE user_id = ? AND id = ?';
  for (const id of ids.slice(0, 200)) run(stmt, nowSec(), userId, String(id));
}

export function expireAllSessions() {
  run('DELETE FROM sessions WHERE expires_at <= ?', nowSec());
}

export function requireUser(req) {
  const u = userFromToken(bearer(req));
  if (!u || u.banned) return null;
  return getUserById(u.id);
}
