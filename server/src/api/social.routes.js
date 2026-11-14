// Friends, blocks, profiles, reports, chat history.
import { get, run, all } from '../core/db.js';
import { newId } from '../core/security.js';
import { badRequest, conflict, notFound, forbidden, nowSec, clampInt, rateLimit, cleanText } from '../core/util.js';
import { notify } from '../core/auth.js';
import { profileUser, publicUser, safeJson } from '../core/users.js';
import { sanitizeChat } from '../core/moderation.js';
import { get as getRoute, post, del } from './router.js';
import { isOnline, friendIdsOf } from '../core/realtime.js';

getRoute('/api/users/search', async (ctx) => {
  const q = (ctx.query.get('q') || '').trim();
  if (q.length < 2) return ctx.out(200, { ok: true, users: [] });
  const rows = all(
    'SELECT * FROM users WHERE (username LIKE ? OR email LIKE ?) AND banned = 0 AND id != ? ORDER BY username LIMIT 20',
    `%${q}%`, `%${q}%`, ctx.user?.id || ''
  );
  ctx.out(200, { ok: true, users: rows.map((u) => publicUser(u, { online: isOnline(u.id) })) });
}, { auth: true });

getRoute('/api/users/:id', async (ctx) => {
  const u = get('SELECT * FROM users WHERE id = ?', ctx.params.id);
  if (!u) throw notFound('المستخدم غير موجود');
  if (u.banned && ctx.user?.role !== 'owner') throw notFound('المستخدم غير موجود');
  ctx.out(200, { ok: true, user: profileUser(u, ctx.user?.id), online: isOnline(u.id) });
}, { auth: true });

// ---- friends -------------------------------------------------------------
getRoute('/api/friends', async (ctx) => {
  const rows = all(
    `SELECT f.*, u.id AS uid, u.username, u.avatar, u.role, u.miy
     FROM friendships f
     JOIN users u ON u.id = CASE WHEN f.requester_id = ? THEN f.addressee_id ELSE f.requester_id END
     WHERE (f.requester_id = ? OR f.addressee_id = ?)`,
    ctx.user.id, ctx.user.id, ctx.user.id
  );
  const friends = [];
  const incoming = [];
  const outgoing = [];
  for (const r of rows) {
    const base = { id: r.uid, username: r.username, avatar: r.avatar, role: r.role, online: isOnline(r.uid) };
    if (r.status === 'accepted') friends.push({ ...base, friendshipId: r.id, since: r.updated_at });
    else if (r.status === 'pending') {
      if (r.addressee_id === ctx.user.id) incoming.push({ ...base, friendshipId: r.id, requestedAt: r.created_at });
      else outgoing.push({ ...base, friendshipId: r.id, requestedAt: r.created_at });
    }
  }
  friends.sort((a, b) => Number(b.online) - Number(a.online) || a.username.localeCompare(b.username));
  ctx.out(200, { ok: true, friends, incoming, outgoing });
}, { auth: true });

post('/api/friends/request', async (ctx) => {
  rateLimit(`friendreq:${ctx.user.id}`, 40, 3600000);
  const targetId = String(ctx.body.userId || '');
  if (targetId === ctx.user.id) throw badRequest('self_request', 'لا يمكنك إرسال طلب لنفسك');
  const target = get('SELECT * FROM users WHERE id = ? AND banned = 0', targetId);
  if (!target) throw notFound('المستخدم غير موجود');
  if (get('SELECT 1 AS x FROM blocks WHERE blocker_id = ? AND blocked_id = ?', target.id, ctx.user.id)) {
    throw forbidden('لا يمكن إرسال الطلب');
  }
  if (get('SELECT 1 AS x FROM blocks WHERE blocker_id = ? AND blocked_id = ?', ctx.user.id, target.id)) {
    throw forbidden('قمت بحظر هذا اللاعب');
  }
  const existing = get(
    'SELECT * FROM friendships WHERE (requester_id = ? AND addressee_id = ?) OR (requester_id = ? AND addressee_id = ?)',
    ctx.user.id, target.id, target.id, ctx.user.id
  );
  if (existing) {
    if (existing.status === 'accepted') throw conflict('already_friends', 'أنتما صديقان بالفعل');
    if (existing.status === 'pending') {
      if (existing.addressee_id === ctx.user.id) {
        // mutual request => accept automatically
        run("UPDATE friendships SET status = 'accepted', updated_at = ? WHERE id = ?", nowSec(), existing.id);
        notify(target.id, 'friend', 'صداقة جديدة', `${ctx.user.username} أصبح صديقك`);
        return ctx.out(200, { ok: true, status: 'accepted' });
      }
      throw conflict('already_requested', 'الطلب مُرسل بالفعل');
    }
    run('DELETE FROM friendships WHERE id = ?', existing.id);
  }
  const id = newId('fr');
  run(
    "INSERT INTO friendships(id,requester_id,addressee_id,status,created_at,updated_at) VALUES(?,?,?,'pending',?,?)",
    id, ctx.user.id, target.id, nowSec(), nowSec()
  );
  notify(target.id, 'friend', 'طلب صداقة', `${ctx.user.username} أرسل لك طلب صداقة`, { userId: ctx.user.id });
  ctx.out(201, { ok: true, status: 'pending', friendshipId: id });
}, { auth: true });

post('/api/friends/:id/accept', async (ctx) => {
  const f = get('SELECT * FROM friendships WHERE id = ?', ctx.params.id);
  if (!f) throw notFound('الطلب غير موجود');
  if (f.addressee_id !== ctx.user.id) throw forbidden('هذا الطلب ليس لك');
  if (f.status !== 'pending') throw conflict('not_pending', 'الطلب لم يعد معلقاً');
  run("UPDATE friendships SET status = 'accepted', updated_at = ? WHERE id = ?", nowSec(), f.id);
  notify(f.requester_id, 'friend', 'تم قبول الصداقة', `${ctx.user.username} قبل طلبك`);
  ctx.out(200, { ok: true });
}, { auth: true });

post('/api/friends/:id/decline', async (ctx) => {
  const f = get('SELECT * FROM friendships WHERE id = ?', ctx.params.id);
  if (!f) throw notFound('الطلب غير موجود');
  if (f.addressee_id !== ctx.user.id) throw forbidden('هذا الطلب ليس لك');
  run("UPDATE friendships SET status = 'declined', updated_at = ? WHERE id = ?", nowSec(), f.id);
  ctx.out(200, { ok: true });
}, { auth: true });

del('/api/friends/:id', async (ctx) => {
  const f = get('SELECT * FROM friendships WHERE id = ?', ctx.params.id);
  if (!f) throw notFound('الصداقة غير موجودة');
  if (f.requester_id !== ctx.user.id && f.addressee_id !== ctx.user.id) throw forbidden();
  run('DELETE FROM friendships WHERE id = ?', f.id);
  ctx.out(200, { ok: true });
}, { auth: true });

post('/api/blocks', async (ctx) => {
  const targetId = String(ctx.body.userId || '');
  const target = get('SELECT * FROM users WHERE id = ?', targetId);
  if (!target) throw notFound('المستخدم غير موجود');
  if (target.role === 'owner') throw forbidden('لا يمكن حظر مالك المنصة');
  run('INSERT OR IGNORE INTO blocks(blocker_id,blocked_id,created_at) VALUES(?,?,?)', ctx.user.id, target.id, nowSec());
  run(
    "DELETE FROM friendships WHERE (requester_id = ? AND addressee_id = ?) OR (requester_id = ? AND addressee_id = ?)",
    ctx.user.id, target.id, target.id, ctx.user.id
  );
  ctx.out(200, { ok: true });
}, { auth: true });

del('/api/blocks/:userId', async (ctx) => {
  run('DELETE FROM blocks WHERE blocker_id = ? AND blocked_id = ?', ctx.user.id, ctx.params.userId);
  ctx.out(200, { ok: true });
}, { auth: true });

getRoute('/api/blocks', async (ctx) => {
  const rows = all(
    'SELECT u.id,u.username,u.avatar FROM blocks b JOIN users u ON u.id = b.blocked_id WHERE b.blocker_id = ?', ctx.user.id
  );
  ctx.out(200, { ok: true, blocked: rows });
}, { auth: true });

// ---- reports -------------------------------------------------------------
post('/api/reports', async (ctx) => {
  rateLimit(`report:${ctx.user.id}`, 20, 3600000);
  const targetType = ['user', 'map', 'message', 'pass'].includes(ctx.body.targetType) ? ctx.body.targetType : 'user';
  const targetId = String(ctx.body.targetId || '').slice(0, 80);
  const reason = cleanText(ctx.body.reason, 60, 'السبب');
  const details = cleanText(ctx.body.details || '', 1000, 'التفاصيل');
  if (!targetId || reason.length < 3) throw badRequest('bad_report', 'بيانات البلاغ غير مكتملة');
  const dup = get(
    "SELECT 1 AS x FROM reports WHERE reporter_id = ? AND target_type = ? AND target_id = ? AND status = 'open'",
    ctx.user.id, targetType, targetId
  );
  if (dup) throw conflict('duplicate_report', 'لديك بلاغ مفتوح على هذا العنصر');
  const id = newId('rep');
  run(
    'INSERT INTO reports(id,reporter_id,target_type,target_id,reason,details,status,created_at) VALUES(?,?,?,?,?,?,?,?)',
    id, ctx.user.id, targetType, targetId, reason, details, 'open', nowSec()
  );
  ctx.out(201, { ok: true, reportId: id });
}, { auth: true });

// ---- chat history --------------------------------------------------------
function channelKey(ctx) {
  // "map" | "dm"
  return { channel: ctx.query.get('channel') === 'dm' ? 'dm' : 'map', channelId: String(ctx.query.get('channelId') || '').slice(0, 80) };
}

function assertChannelAccess(ctx, channel, channelId) {
  if (channel === 'map') {
    const m = get('SELECT * FROM maps WHERE id = ?', channelId);
    if (!m) throw notFound('الماب غير موجود');
    return m;
  }
  // DM channel id is "u1:u2" sorted; only participants may read
  const parts = channelId.split(':');
  if (!parts.includes(ctx.user.id) || parts.length !== 2) throw forbidden();
  return null;
}

getRoute('/api/chat/history', async (ctx) => {
  const { channel, channelId } = channelKey(ctx);
  assertChannelAccess(ctx, channel, channelId);
  const limit = clampInt(ctx.query.get('limit'), 1, 100, 50);
  // inclusive upper bound: a message written in the same second must still be returned
  // `before` is a cursor: absent/0 means "newest page". Clamping to min=1 would turn an
  // absent cursor into 1 and return nothing, so the fallback is applied before clamping.
  const beforeRaw = ctx.query.get('before');
  const before = beforeRaw ? clampInt(beforeRaw, 1, 4e12, nowSec()) : nowSec();
  const rows = all(
    `SELECT m.*, u.username, u.avatar FROM messages m JOIN users u ON u.id = m.sender_id
     WHERE m.channel = ? AND m.channel_id = ? AND m.created_at <= ? AND m.deleted = 0
     ORDER BY m.created_at DESC LIMIT ?`, channel, channelId, before, limit
  );
  ctx.out(200, {
    ok: true,
    messages: rows.reverse().map((r) => ({
      id: r.id, body: r.body, createdAt: r.created_at,
      sender: { id: r.sender_id, username: r.username, avatar: r.avatar },
    })),
  });
}, { auth: true });

getRoute('/api/chat/threads', async (ctx) => {
  const me = ctx.user.id;
  const rows = all(
    `SELECT channel_id, MAX(created_at) AS last_at, COUNT(*) AS count FROM messages
     WHERE channel = 'dm' AND channel_id LIKE ? AND deleted = 0 GROUP BY channel_id ORDER BY last_at DESC LIMIT 40`,
    `%${me}%`
  );
  const threads = [];
  for (const r of rows) {
    const parts = r.channel_id.split(':');
    if (!parts.includes(me)) continue;
    const otherId = parts.find((p) => p !== me);
    const other = get('SELECT id,username,avatar FROM users WHERE id = ?', otherId);
    if (!other) continue;
    const last = get(
      "SELECT body FROM messages WHERE channel = 'dm' AND channel_id = ? AND deleted = 0 ORDER BY created_at DESC LIMIT 1",
      r.channel_id
    );
    threads.push({
      channelId: r.channel_id, lastMessage: last?.body || '', lastAt: r.last_at, count: r.count,
      user: { id: other.id, username: other.username, avatar: other.avatar, online: isOnline(other.id) },
    });
  }
  ctx.out(200, { ok: true, threads });
}, { auth: true });

export function dmChannelId(a, b) {
  return [a, b].sort().join(':');
}

post('/api/chat/dm/:userId/read', async (ctx) => {
  const cid = dmChannelId(ctx.user.id, ctx.params.userId);
  const rows = all("SELECT * FROM messages WHERE channel = 'dm' AND channel_id = ? ORDER BY created_at DESC LIMIT 50", cid);
  ctx.out(200, {
    ok: true,
    channelId: cid,
    messages: rows.reverse().map((r) => {
      const sender = get('SELECT id,username,avatar FROM users WHERE id = ?', r.sender_id);
      return { id: r.id, body: r.body, createdAt: r.created_at, sender };
    }),
  });
}, { auth: true });
