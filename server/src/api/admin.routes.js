// Owner dashboard: users, maps, reports, economy, passes, roles, server settings, AI settings,
// statistics, audit log. Every mutating action writes an audit row.
import { get, run, all, getSetting, setSetting } from '../core/db.js';
import { newId, hashPassword } from '../core/security.js';
import { badRequest, notFound, conflict, nowSec, clampInt, cleanText, forbidden } from '../core/util.js';
import { audit, notify, destroyUserSessions } from '../core/auth.js';
import { publicUser, safeJson } from '../core/users.js';
import { credit, ledgerFor } from '../core/economy.js';
import { realtimeStats } from '../core/realtime.js';
import { config } from '../core/config.js';
import fs from 'node:fs';
import { get as getRoute, post, patch, del } from './router.js';
import { SIZE_PRESETS } from '../ai/world.js';

const O = { auth: true, owner: true };

getRoute('/api/admin/overview', async (ctx) => {
  const count = (sql, ...p) => get(sql, ...p).c;
  const now = nowSec();
  const dayAgo = now - 86400;
  const rt = realtimeStats();
  ctx.out(200, {
    ok: true,
    users: {
      total: count('SELECT COUNT(*) AS c FROM users'),
      banned: count('SELECT COUNT(*) AS c FROM users WHERE banned = 1'),
      newToday: count('SELECT COUNT(*) AS c FROM users WHERE created_at >= ?', dayAgo),
      online: rt.onlineUsers,
    },
    maps: {
      total: count('SELECT COUNT(*) AS c FROM maps'),
      published: count("SELECT COUNT(*) AS c FROM maps WHERE status = 'published'"),
      ai: count('SELECT COUNT(*) AS c FROM maps WHERE is_ai = 1'),
      visits: get('SELECT COALESCE(SUM(visits),0) AS c FROM maps').c,
    },
    economy: {
      circulating: get('SELECT COALESCE(SUM(miy),0) AS c FROM users').c,
      minted: get('SELECT COALESCE(SUM(delta),0) AS c FROM transactions WHERE delta > 0').c,
      spent: get("SELECT COALESCE(-SUM(delta),0) AS c FROM transactions WHERE delta < 0").c,
      transactions: count('SELECT COUNT(*) AS c FROM transactions'),
      minutesToday: get(
        'SELECT COALESCE(SUM(credited_minutes),0) AS c FROM map_sessions WHERE started_at >= ?',
        Math.floor(new Date().setUTCHours(0, 0, 0, 0) / 1000)
      ).c,
    },
    reports: { open: count("SELECT COUNT(*) AS c FROM reports WHERE status = 'open'") },
    sessions: { active: count('SELECT COUNT(*) AS c FROM sessions WHERE expires_at > ?', now) },
    realtime: rt,
    server: {
      uptime: Math.round(process.uptime()),
      memoryMB: Math.round(process.memoryUsage().rss / 1048576),
      version: config.version,
      ai: config.ai.baseUrl ? 'remote' : 'local-planner',
      db: 'sqlite',
    },
    recentAudit: all('SELECT * FROM audit_log ORDER BY created_at DESC LIMIT 12').map(auditRow),
    topMaps: all("SELECT id,name,visits FROM maps WHERE status='published' ORDER BY visits DESC LIMIT 5"),
  });
}, O);

function auditRow(a) {
  return { id: a.id, actorId: a.actor_id, action: a.action, target: a.target,
    detail: safeJson(a.detail, {}), ip: a.ip, at: a.created_at };
}

// ---- broadcast -----------------------------------------------------------
// The owner console sends one notification to every active player. Announcements
// travel through the normal notifications feed, so they survive a restart.
post('/api/admin/broadcast', async (ctx) => {
  const title = cleanText(ctx.body.title, 60) || 'NEXORA';
  const body = cleanText(ctx.body.body, 400);
  if (!body) throw badRequest('empty_body', 'لا يمكن إرسال إشعار فارغ');
  const users = all('SELECT id FROM users WHERE banned = 0');
  for (const u of users) notify(u.id, 'system', title, body, { broadcast: true });
  audit(ctx.user.id, 'admin.broadcast', 'users', { recipients: users.length }, ctx.ip);
  ctx.out(200, { ok: true, sent: users.length });
}, O);

// ---- users ---------------------------------------------------------------
getRoute('/api/admin/users', async (ctx) => {
  const q = (ctx.query.get('q') || '').trim();
  const limit = clampInt(ctx.query.get('limit'), 1, 100, 40);
  const offset = clampInt(ctx.query.get('offset'), 0, 100000, 0);
  let where = '1=1'; const params = [];
  if (q) { where += ' AND (username LIKE ? OR email LIKE ? OR id = ?)'; params.push(`%${q}%`, `%${q}%`, q); }
  const rows = all(`SELECT * FROM users WHERE ${where} ORDER BY created_at DESC LIMIT ? OFFSET ?`, ...params, limit, offset);
  ctx.out(200, {
    ok: true,
    total: get(`SELECT COUNT(*) AS c FROM users WHERE ${where}`, ...params).c,
    users: rows.map((u) => ({
      ...publicUser(u),
      email: u.email,
      banned: !!u.banned,
      banReason: u.ban_reason,
      lastLogin: u.last_login,
      transactions: get('SELECT COUNT(*) AS c FROM transactions WHERE user_id = ?', u.id).c,
      maps: get('SELECT COUNT(*) AS c FROM maps WHERE owner_id = ?', u.id).c,
    })),
  });
}, O);

post('/api/admin/users/:id/ban', async (ctx) => {
  const u = get('SELECT * FROM users WHERE id = ?', ctx.params.id);
  if (!u) throw notFound('المستخدم غير موجود');
  if (u.role === 'owner') throw forbidden('لا يمكن حظر المالك');
  const reason = cleanText(ctx.body.reason || 'مخالفة الشروط', 300);
  const ban = ctx.body.ban !== false;
  run('UPDATE users SET banned = ?, ban_reason = ? WHERE id = ?', ban ? 1 : 0, ban ? reason : '', u.id);
  if (ban) {
    destroyUserSessions(u.id);
    notify(u.id, 'system', 'تم حظر الحساب', reason);
  } else {
    notify(u.id, 'system', 'تم فك الحظر', 'يمكنك الدخول مجدداً');
  }
  audit(ctx.user.id, ban ? 'user.ban' : 'user.unban', u.id, { reason }, ctx.ip);
  ctx.out(200, { ok: true, banned: ban });
}, O);

post('/api/admin/users/:id/role', async (ctx) => {
  const u = get('SELECT * FROM users WHERE id = ?', ctx.params.id);
  if (!u) throw notFound('المستخدم غير موجود');
  if (u.role === 'owner') throw forbidden('لا يمكن تغيير دور المالك');
  const role = String(ctx.body.role || '');
  if (!['player', 'creator', 'moderator'].includes(role)) throw badRequest('bad_role', 'دور غير صالح');
  run('UPDATE users SET role = ? WHERE id = ?', role, u.id);
  audit(ctx.user.id, 'user.role', u.id, { role, previous: u.role }, ctx.ip);
  notify(u.id, 'system', 'تم تحديث صلاحياتك', `دورك الجديد: ${role}`);
  ctx.out(200, { ok: true, role });
}, O);

post('/api/admin/users/:id/grant', async (ctx) => {
  const u = get('SELECT * FROM users WHERE id = ?', ctx.params.id);
  if (!u) throw notFound('المستخدم غير موجود');
  const amount = clampInt(ctx.body.amount, -1000000, 1000000, 0);
  if (!amount) throw badRequest('bad_amount', 'المبلغ غير صالح');
  const reason = cleanText(ctx.body.reason || 'تعديل إداري', 200);
  const res = credit(u.id, amount, 'admin_adjust', `admin:${newId('adj')}`, { by: ctx.user.id, reason });
  audit(ctx.user.id, 'economy.adjust', u.id, { amount, reason }, ctx.ip);
  notify(u.id, 'economy', amount > 0 ? `+${amount} Miy` : `${amount} Miy`, reason);
  ctx.out(200, { ok: true, balance: res.balance });
}, O);

post('/api/admin/users/:id/password', async (ctx) => {
  const u = get('SELECT * FROM users WHERE id = ?', ctx.params.id);
  if (!u) throw notFound('المستخدم غير موجود');
  const pw = String(ctx.body.password || '');
  if (pw.length < 10) throw badRequest('weak_password', 'كلمة مرور الإدارة يجب أن تكون 10 أحرف على الأقل');
  run('UPDATE users SET password_hash = ? WHERE id = ?', hashPassword(pw), u.id);
  destroyUserSessions(u.id);
  audit(ctx.user.id, 'user.password_reset', u.id, {}, ctx.ip);
  ctx.out(200, { ok: true });
}, O);

// ---- maps ----------------------------------------------------------------
getRoute('/api/admin/maps', async (ctx) => {
  const limit = clampInt(ctx.query.get('limit'), 1, 100, 40);
  const rows = all(
    `SELECT m.*, u.username FROM maps m JOIN users u ON u.id = m.owner_id ORDER BY m.updated_at DESC LIMIT ?`, limit
  );
  ctx.out(200, {
    ok: true,
    maps: rows.map((m) => ({
      id: m.id, name: m.name, owner: m.username, ownerId: m.owner_id, gameType: m.game_type,
      status: m.status, privacy: m.privacy, visits: m.visits, version: m.version, isAi: !!m.is_ai,
      updatedAt: m.updated_at,
      likes: get('SELECT COUNT(*) AS c FROM map_likes WHERE map_id = ?', m.id).c,
    })),
  });
}, O);

del('/api/admin/maps/:id', async (ctx) => {
  const m = get('SELECT * FROM maps WHERE id = ?', ctx.params.id);
  if (!m) throw notFound('الماب غير موجود');
  const reason = cleanText(ctx.body.reason || 'مخالفة', 300);
  run('DELETE FROM maps WHERE id = ?', m.id);
  audit(ctx.user.id, 'map.remove', m.id, { name: m.name, reason, owner: m.owner_id }, ctx.ip);
  notify(m.owner_id, 'system', 'تم حذف ماب', `«${m.name}» — السبب: ${reason}`);
  ctx.out(200, { ok: true });
}, O);

// ---- reports -------------------------------------------------------------
getRoute('/api/admin/reports', async (ctx) => {
  const status = ['open', 'resolved', 'dismissed'].includes(ctx.query.get('status')) ? ctx.query.get('status') : 'open';
  const rows = all('SELECT * FROM reports WHERE status = ? ORDER BY created_at DESC LIMIT 100', status);
  ctx.out(200, {
    ok: true,
    reports: rows.map((r) => {
      const reporter = get('SELECT username FROM users WHERE id = ?', r.reporter_id);
      let target = null;
      if (r.target_type === 'user') target = get('SELECT id,username FROM users WHERE id = ?', r.target_id);
      if (r.target_type === 'map') target = get('SELECT id,name AS username FROM maps WHERE id = ?', r.target_id);
      if (r.target_type === 'message') {
        const msg = get('SELECT * FROM messages WHERE id = ?', r.target_id);
        target = msg ? { id: msg.id, username: msg.body.slice(0, 80), channel: msg.channel } : null;
      }
      return {
        id: r.id, targetType: r.target_type, targetId: r.target_id, reason: r.reason, details: r.details,
        status: r.status, createdAt: r.created_at, reporter: reporter?.username || '—', target,
      };
    }),
  });
}, O);

post('/api/admin/reports/:id/resolve', async (ctx) => {
  const r = get('SELECT * FROM reports WHERE id = ?', ctx.params.id);
  if (!r) throw notFound('البلاغ غير موجود');
  const action = String(ctx.body.action || 'dismiss');
  const status = ctx.body.status === 'dismissed' ? 'dismissed' : 'resolved';
  if (action === 'ban_user' && r.target_type === 'user') {
    const t = get('SELECT * FROM users WHERE id = ?', r.target_id);
    if (t && t.role !== 'owner') {
      run('UPDATE users SET banned = 1, ban_reason = ? WHERE id = ?', `بلاغ: ${r.reason}`, t.id);
      destroyUserSessions(t.id);
    }
  }
  if (action === 'remove_map' && r.target_type === 'map') {
    const m = get('SELECT * FROM maps WHERE id = ?', r.target_id);
    if (m) {
      run("UPDATE maps SET status = 'draft', privacy = 'private' WHERE id = ?", m.id);
      notify(m.owner_id, 'system', 'تم إخفاء ماب', `«${m.name}» — ${r.reason}`);
    }
  }
  if (action === 'delete_message' && r.target_type === 'message') {
    run('UPDATE messages SET deleted = 1 WHERE id = ?', r.target_id);
  }
  run('UPDATE reports SET status = ?, reviewed_by = ?, reviewed_at = ?, action = ? WHERE id = ?',
    status, ctx.user.id, nowSec(), action, r.id);
  audit(ctx.user.id, 'report.review', r.id, { action, status }, ctx.ip);
  ctx.out(200, { ok: true });
}, O);

// ---- economy / passes ----------------------------------------------------
getRoute('/api/admin/transactions', async (ctx) => {
  const limit = clampInt(ctx.query.get('limit'), 1, 200, 80);
  const rows = all(
    `SELECT t.*, u.username FROM transactions t JOIN users u ON u.id = t.user_id
     ORDER BY t.created_at DESC LIMIT ?`, limit
  );
  ctx.out(200, {
    ok: true,
    entries: rows.map((t) => ({
      id: t.id, user: t.username, userId: t.user_id, delta: t.delta, balance: t.balance_after,
      reason: t.reason, at: t.created_at, meta: safeJson(t.meta, {}),
    })),
  });
}, O);

getRoute('/api/admin/leaderboard', async (ctx) => {
  ctx.out(200, {
    ok: true,
    byPlaytime: all('SELECT username, play_seconds FROM users ORDER BY play_seconds DESC LIMIT 10'),
    byMiy: all('SELECT username, miy FROM users ORDER BY miy DESC LIMIT 10'),
  });
}, O);

getRoute('/api/admin/passes', async (ctx) => {
  const rows = all(
    `SELECT g.*, u.username AS creator, m.name AS map_name,
      (SELECT COUNT(*) FROM pass_ownership o WHERE o.pass_id = g.id) AS sales,
      (SELECT COALESCE(SUM(price_paid),0) FROM pass_ownership o WHERE o.pass_id = g.id) AS revenue
     FROM game_passes g JOIN users u ON u.id = g.creator_id JOIN maps m ON m.id = g.map_id
     ORDER BY g.created_at DESC LIMIT 100`
  );
  ctx.out(200, {
    ok: true,
    passes: rows.map((p) => ({
      id: p.id, name: p.name, mapId: p.map_id, mapName: p.map_name, creator: p.creator,
      price: p.price, sales: p.sales, revenue: p.revenue, published: !!p.published, createdAt: p.created_at,
    })),
  });
}, O);

patch('/api/admin/passes/:id', async (ctx) => {
  const p = get('SELECT * FROM game_passes WHERE id = ?', ctx.params.id);
  if (!p) throw notFound('الألباس غير موجود');
  if (ctx.body.published !== undefined) {
    run('UPDATE game_passes SET published = ? WHERE id = ?', ctx.body.published ? 1 : 0, p.id);
  }
  if (ctx.body.price !== undefined) {
    run('UPDATE game_passes SET price = ? WHERE id = ?', clampInt(ctx.body.price, 0, 100000, p.price), p.id);
  }
  audit(ctx.user.id, 'pass.update', p.id, ctx.body, ctx.ip);
  ctx.out(200, { ok: true });
}, O);

// ---- audit / security log -------------------------------------------------
getRoute('/api/admin/audit', async (ctx) => {
  const limit = clampInt(ctx.query.get('limit'), 1, 300, 100);
  const rows = all('SELECT * FROM audit_log ORDER BY created_at DESC LIMIT ?', limit);
  const names = new Map();
  for (const r of rows) {
    if (r.actor_id && !names.has(r.actor_id)) {
      const u = get('SELECT username FROM users WHERE id = ?', r.actor_id);
      names.set(r.actor_id, u?.username || r.actor_id);
    }
  }
  ctx.out(200, {
    ok: true,
    entries: rows.map((a) => ({ ...auditRow(a), actor: names.get(a.actor_id) || 'system' })),
  });
}, O);

// ---- settings ------------------------------------------------------------
const ALLOWED_SETTINGS = ['motd', 'maintenance', 'registrationOpen', 'maxPlayersDefault', 'aiProvider', 'aiModel', 'aiDailyQuota'];

getRoute('/api/admin/settings', async (ctx) => {
  const settings = {};
  for (const k of ALLOWED_SETTINGS) settings[k] = getSetting(k, null);
  ctx.out(200, {
    ok: true,
    settings,
    ai: {
      provider: config.ai.baseUrl ? 'remote' : 'local-planner',
      model: config.ai.model || 'nexora-local-v1',
      hasApiKey: !!config.ai.apiKey,
      note: config.ai.baseUrl
        ? 'يستخدم مزوّداً خارجياً عند توفره، مع المخطط المحلي كخطة بديلة.'
        : 'المخطط المحلي يعمل دائماً بدون إنترنت ولا يحتاج مفتاحاً.',
    },
    economy: { miyPerMinute: 5, dailyCapMinutes: config.economy.maxMinutesPerDay },
    worldSizes: Object.keys(SIZE_PRESETS),
  });
}, O);

patch('/api/admin/settings', async (ctx) => {
  const changed = {};
  for (const [k, v] of Object.entries(ctx.body || {})) {
    if (!ALLOWED_SETTINGS.includes(k)) continue;
    setSetting(k, v);
    changed[k] = v;
  }
  audit(ctx.user.id, 'settings.update', 'server', changed, ctx.ip);
  ctx.out(200, { ok: true, changed });
}, O);

getRoute('/api/admin/health', async (ctx) => {
  ctx.out(200, {
    ok: true,
    uptime: process.uptime(),
    memory: process.memoryUsage(),
    realtime: realtimeStats(),
    db: { path: config.dbPath, sizeBytes: (() => {
      try { return fs.statSync(config.dbPath).size; } catch { return 0; }
    })() },
    node: process.version,
  });
}, O);
