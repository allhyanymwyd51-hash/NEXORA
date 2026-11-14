// Maps: creation, listing, play sessions, ratings, favorites, game passes.
import crypto from 'node:crypto';
import { get, run, all } from '../core/db.js';
import { newId } from '../core/security.js';
import {
  badRequest, conflict, notFound, forbidden, nowSec, cleanText, clampInt, rateLimit,
} from '../core/util.js';
import { notify } from '../core/auth.js';
import { publicUser, safeJson } from '../core/users.js';
import { GAME_TYPES, validateWorld, SIZE_PRESETS, DETAIL_MULT } from '../ai/world.js';
import { startPlaySession, endPlaySession, heartbeat } from '../core/economy.js';
import { get as getRoute, post, patch, del } from './router.js';
import { containsProfanity } from '../core/moderation.js';
import { roomState } from '../core/realtime.js';

const PUBLIC_COLUMNS = `m.id, m.owner_id, m.name, m.description, m.cover, m.game_type, m.privacy,
  m.max_players, m.version, m.status, m.visits, m.is_ai, m.created_at, m.updated_at, m.published_at`;

function mapCard(row, viewerId) {
  const owner = get('SELECT * FROM users WHERE id = ?', row.owner_id);
  const likes = get('SELECT COUNT(*) AS c FROM map_likes WHERE map_id = ?', row.id).c;
  const liked = viewerId ? !!get('SELECT 1 AS x FROM map_likes WHERE map_id = ? AND user_id = ?', row.id, viewerId) : false;
  const fav = viewerId ? !!get('SELECT 1 AS x FROM map_favorites WHERE map_id = ? AND user_id = ?', row.id, viewerId) : false;
  const passes = get('SELECT COUNT(*) AS c FROM game_passes WHERE map_id = ? AND published = 1', row.id).c;
  const world = safeJson(row.world || '{}', {});
  return {
    id: row.id,
    name: row.name,
    description: row.description,
    cover: row.cover || '',
    gameType: row.game_type,
    privacy: row.privacy,
    maxPlayers: row.max_players,
    players: roomState(row.id).count,   // live room occupancy, so cards can show "3 playing"
    version: row.version,
    status: row.status,
    visits: row.visits,
    likes,
    liked,
    favorite: fav,
    isAi: !!row.is_ai,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
    publishedAt: row.published_at,
    passes,
    owner: owner ? { id: owner.id, username: owner.username, avatar: owner.avatar } : null,
    // lightweight preview indicators only (no full payload in list responses)
    worldSummary: {
      size: world.size || 'medium',
      side: world.side || 0,
      features: (world.features || []).slice(0, 6),
      weather: world.environment?.weather || 'clear',
      stats: world.stats ? { trees: world.stats.trees, buildings: world.stats.buildings, objects: world.stats.objects } : null,
    },
    spawn: world.spawns?.[0] || { x: 0, z: 0 },
  };
}

function mapDetail(row, viewerId) {
  const card = mapCard(row, viewerId);
  const world = safeJson(row.world || '{}', {});
  return {
    ...card,
    world,
    ownerDetails: (() => {
      const o = get('SELECT * FROM users WHERE id = ?', row.owner_id);
      return o ? publicUser(o) : null;
    })(),
    passesList: all('SELECT * FROM game_passes WHERE map_id = ? AND published = 1 ORDER BY created_at DESC', row.id)
      .map((p) => ({
        id: p.id, name: p.name, description: p.description, price: p.price, image: p.image,
        perks: safeJson(p.perks, []), owned: viewerId
          ? !!get('SELECT 1 AS x FROM pass_ownership WHERE pass_id = ? AND user_id = ?', p.id, viewerId) : false,
      })),
  };
}

function ownMap(mapId, user, { allowOwnerRole = false } = {}) {
  const m = get('SELECT * FROM maps WHERE id = ?', mapId);
  if (!m) throw notFound('الماب غير موجود');
  const isOwn = m.owner_id === user.id;
  if (!isOwn && !allowOwnerRole && user.role !== 'owner' && user.role !== 'moderator') throw forbidden('لا تملك هذا الماب');
  return m;
}

// ---- listing --------------------------------------------------------------
getRoute('/api/maps', async (ctx) => {
  const sort = ctx.query.get('sort') || 'new';
  const q = (ctx.query.get('q') || '').trim().slice(0, 60);
  const type = ctx.query.get('type');
  const limit = clampInt(ctx.query.get('limit'), 1, 50, 24);
  const offset = clampInt(ctx.query.get('offset'), 0, 5000, 0);
  let where = "m.status = 'published' AND m.privacy = 'public'";
  const params = [];
  if (q) { where += ' AND (m.name LIKE ? OR m.description LIKE ?)'; params.push(`%${q}%`, `%${q}%`); }
  if (type && GAME_TYPES.includes(type)) { where += ' AND m.game_type = ?'; params.push(type); }
  // "players" is live occupancy, which only exists in the realtime hub — take the
  // candidate window ordered by visits, then order it by who is actually playing.
  const liveSort = sort === 'players';
  const order = liveSort ? 'm.visits DESC'
    : sort === 'visits' ? 'm.visits DESC'
      : sort === 'likes' ? '(SELECT COUNT(*) FROM map_likes l WHERE l.map_id = m.id) DESC, m.published_at DESC'
        : 'm.published_at DESC';
  const rows = all(`SELECT ${PUBLIC_COLUMNS} FROM maps m WHERE ${where} ORDER BY ${order} LIMIT ? OFFSET ?`,
    ...params, liveSort ? Math.min(200, offset + limit) : limit, liveSort ? 0 : offset);
  const cards = rows.map((r) => mapCard(r, ctx.user?.id));
  if (liveSort) cards.sort((a, b) => b.players - a.players || b.visits - a.visits);
  const total = get(`SELECT COUNT(*) AS c FROM maps m WHERE ${where}`, ...params).c;
  ctx.out(200, {
    ok: true, total, offset, limit,
    maps: liveSort ? cards.slice(offset, offset + limit) : cards,
  });
});

getRoute('/api/maps/mine', async (ctx) => {
  const rows = all(`SELECT ${PUBLIC_COLUMNS} FROM maps m WHERE m.owner_id = ? ORDER BY m.updated_at DESC`, ctx.user.id);
  ctx.out(200, { ok: true, maps: rows.map((r) => mapCard(r, ctx.user.id)) });
}, { auth: true });

getRoute('/api/maps/favorites', async (ctx) => {
  const rows = all(
    `SELECT ${PUBLIC_COLUMNS} FROM maps m JOIN map_favorites f ON f.map_id = m.id
     WHERE f.user_id = ? ORDER BY f.created_at DESC`, ctx.user.id
  );
  ctx.out(200, { ok: true, maps: rows.map((r) => mapCard(r, ctx.user.id)) });
}, { auth: true });

getRoute('/api/maps/:id', async (ctx) => {
  const m = get('SELECT * FROM maps WHERE id = ?', ctx.params.id);
  if (!m) throw notFound('الماب غير موجود');
  const isPublic = m.status === 'published' && m.privacy === 'public';
  const isOwnerViewer = ctx.user && (ctx.user.id === m.owner_id || ctx.user.role === 'owner' || ctx.user.role === 'moderator');
  if (!isPublic && !isOwnerViewer) throw forbidden('هذا الماب غير منشور');
  ctx.out(200, { ok: true, map: mapDetail(m, ctx.user?.id) });
});

// ---- creation / editing ---------------------------------------------------
post('/api/maps', async (ctx) => {
  rateLimit(`mapcreate:${ctx.user.id}`, 20, 3600000);
  const name = cleanText(ctx.body.name, 60, 'اسم الماب');
  if (name.length < 3) throw badRequest('bad_name', 'اسم الماب قصير جداً');
  if (containsProfanity(name)) throw badRequest('bad_name', 'اسم الماب يحتوي كلمات غير لائقة');
  const description = cleanText(ctx.body.description, 800, 'الوصف');
  const gameType = GAME_TYPES.includes(ctx.body.gameType) ? ctx.body.gameType : 'open_world';
  const privacy = ctx.body.privacy === 'private' ? 'private' : 'public';
  const maxPlayers = clampInt(ctx.body.maxPlayers, 1, 60, 18);
  const isAi = ctx.body.isAi ? 1 : 0;
  const world = ctx.body.world && typeof ctx.body.world === 'object' ? ctx.body.world : null;
  if (world) {
    const errs = validateWorld(world);
    if (errs.length) throw badRequest('bad_world', `بيانات العالم غير صالحة: ${errs.join(', ')}`);
  }
  const now = nowSec();
  const id = newId('map');
  run(
    `INSERT INTO maps(id,owner_id,name,description,cover,game_type,privacy,max_players,version,world,status,is_ai,created_at,updated_at)
     VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)`,
    id, ctx.user.id, name, description, String(ctx.body.cover || '').slice(0, 600000), gameType, privacy,
    maxPlayers, 1, JSON.stringify(world || { format: 'nexora-world/1', spawns: [{ x: 0, z: 0, facing: 0 }], side: 400,
      terrain: { peaks: [], basins: [], rivers: [], baseHeight: 2, roughness: 1, octaves: 3, mountainAmplitude: 10 },
      buildings: [], quests: [], environment: { timeOfDay: 12 }, features: [], stats: {} }),
    'draft', isAi, now, now
  );
  ctx.out(201, { ok: true, map: mapDetail(get('SELECT * FROM maps WHERE id = ?', id), ctx.user.id) });
}, { auth: true });

patch('/api/maps/:id', async (ctx) => {
  const m = ownMap(ctx.params.id, ctx.user);
  const fields = [];
  const values = [];
  if (ctx.body.name !== undefined) {
    const name = cleanText(ctx.body.name, 60, 'اسم الماب');
    if (name.length < 3) throw badRequest('bad_name', 'اسم الماب قصير جداً');
    if (containsProfanity(name)) throw badRequest('bad_name', 'اسم الماب يحتوي كلمات غير لائقة');
    fields.push('name = ?'); values.push(name);
  }
  if (ctx.body.description !== undefined) { fields.push('description = ?'); values.push(cleanText(ctx.body.description, 800, 'الوصف')); }
  if (ctx.body.cover !== undefined) {
    const cover = String(ctx.body.cover || '');
    if (cover.length > 600000) throw badRequest('bad_cover', 'حجم صورة الغلاف كبير جداً');
    if (cover && !/^data:image\/(png|jpe?g|webp);base64,/.test(cover)) throw badRequest('bad_cover', 'صيغة الصورة غير مدعومة');
    fields.push('cover = ?'); values.push(cover);
  }
  if (ctx.body.gameType !== undefined) {
    if (!GAME_TYPES.includes(ctx.body.gameType)) throw badRequest('bad_type', 'نوع اللعبة غير مدعوم');
    fields.push('game_type = ?'); values.push(ctx.body.gameType);
  }
  if (ctx.body.privacy !== undefined) {
    fields.push('privacy = ?'); values.push(ctx.body.privacy === 'private' ? 'private' : 'public');
  }
  if (ctx.body.maxPlayers !== undefined) {
    fields.push('max_players = ?'); values.push(clampInt(ctx.body.maxPlayers, 1, 60, m.max_players));
  }
  if (ctx.body.world !== undefined) {
    const errs = validateWorld(ctx.body.world);
    if (errs.length) throw badRequest('bad_world', `بيانات العالم غير صالحة: ${errs.join(', ')}`);
    fields.push('world = ?'); values.push(JSON.stringify(ctx.body.world));
    fields.push('version = version + 1');
  }
  if (!fields.length) return ctx.out(200, { ok: true, map: mapDetail(m, ctx.user.id) });
  fields.push('updated_at = ?'); values.push(nowSec());
  values.push(m.id);
  run(`UPDATE maps SET ${fields.join(', ')} WHERE id = ?`, ...values);
  ctx.out(200, { ok: true, map: mapDetail(get('SELECT * FROM maps WHERE id = ?', m.id), ctx.user.id) });
}, { auth: true });

post('/api/maps/:id/publish', async (ctx) => {
  const m = ownMap(ctx.params.id, ctx.user);
  const world = safeJson(m.world, {});
  const errs = validateWorld(world);
  if (errs.length) throw badRequest('world_incomplete', `لا يمكن النشر قبل اكتمال العالم: ${errs.join(', ')}`);
  const privacy = ctx.body.privacy === 'private' ? 'private' : 'public';
  run("UPDATE maps SET status = 'published', privacy = ?, published_at = ?, updated_at = ? WHERE id = ?",
    privacy, nowSec(), nowSec(), m.id);
  ctx.out(200, { ok: true, map: mapDetail(get('SELECT * FROM maps WHERE id = ?', m.id), ctx.user.id) });
}, { auth: true });

post('/api/maps/:id/unpublish', async (ctx) => {
  const m = ownMap(ctx.params.id, ctx.user);
  run("UPDATE maps SET status = 'draft', updated_at = ? WHERE id = ?", nowSec(), m.id);
  ctx.out(200, { ok: true });
}, { auth: true });

del('/api/maps/:id', async (ctx) => {
  const m = ownMap(ctx.params.id, ctx.user, { allowOwnerRole: true });
  const reason = cleanText(ctx.body?.reason || '', 300);
  run('INSERT INTO audit_log(id,actor_id,action,target,detail,ip,created_at) VALUES(?,?,?,?,?,?,?)',
    newId('aud'), ctx.user.id, 'map.delete', m.id, JSON.stringify({ name: m.name, reason }), ctx.ip, nowSec());
  run('DELETE FROM maps WHERE id = ?', m.id);
  ctx.out(200, { ok: true });
}, { auth: true });

// ---- ratings / favorites / follows / share -------------------------------
post('/api/maps/:id/like', async (ctx) => {
  const m = get('SELECT * FROM maps WHERE id = ?', ctx.params.id);
  if (!m) throw notFound('الماب غير موجود');
  if (m.owner_id === ctx.user.id) throw badRequest('self_like', 'لا يمكنك الإعجاب بمابك');
  const existing = get('SELECT 1 AS x FROM map_likes WHERE map_id = ? AND user_id = ?', m.id, ctx.user.id);
  if (existing) {
    run('DELETE FROM map_likes WHERE map_id = ? AND user_id = ?', m.id, ctx.user.id);
    return ctx.out(200, { ok: true, liked: false, likes: get('SELECT COUNT(*) AS c FROM map_likes WHERE map_id = ?', m.id).c });
  }
  run('INSERT INTO map_likes(map_id,user_id,created_at) VALUES(?,?,?)', m.id, ctx.user.id, nowSec());
  if (m.owner_id !== ctx.user.id) {
    notify(m.owner_id, 'system', 'إعجاب جديد', `${ctx.user.username} أعجب بماب «${m.name}»`, { mapId: m.id });
  }
  ctx.out(200, { ok: true, liked: true, likes: get('SELECT COUNT(*) AS c FROM map_likes WHERE map_id = ?', m.id).c });
}, { auth: true });

post('/api/maps/:id/favorite', async (ctx) => {
  const m = get('SELECT * FROM maps WHERE id = ?', ctx.params.id);
  if (!m) throw notFound('الماب غير موجود');
  const existing = get('SELECT 1 AS x FROM map_favorites WHERE map_id = ? AND user_id = ?', m.id, ctx.user.id);
  if (existing) {
    run('DELETE FROM map_favorites WHERE map_id = ? AND user_id = ?', m.id, ctx.user.id);
    return ctx.out(200, { ok: true, favorite: false });
  }
  run('INSERT INTO map_favorites(map_id,user_id,created_at) VALUES(?,?,?)', m.id, ctx.user.id, nowSec());
  ctx.out(200, { ok: true, favorite: true });
}, { auth: true });

post('/api/users/:id/follow', async (ctx) => {
  if (ctx.params.id === ctx.user.id) throw badRequest('self_follow', 'لا يمكنك متابعة نفسك');
  const target = get('SELECT * FROM users WHERE id = ?', ctx.params.id);
  if (!target) throw notFound('المستخدم غير موجود');
  const existing = get('SELECT 1 AS x FROM follows WHERE follower_id = ? AND following_id = ?', ctx.user.id, target.id);
  if (existing) {
    run('DELETE FROM follows WHERE follower_id = ? AND following_id = ?', ctx.user.id, target.id);
    return ctx.out(200, { ok: true, following: false });
  }
  run('INSERT INTO follows(follower_id,following_id,created_at) VALUES(?,?,?)', ctx.user.id, target.id, nowSec());
  notify(target.id, 'system', 'متابع جديد', `${ctx.user.username} بدأ متابعتك`);
  ctx.out(200, { ok: true, following: true });
}, { auth: true });

getRoute('/api/users/:id/maps', async (ctx) => {
  const rows = all(
    `SELECT ${PUBLIC_COLUMNS} FROM maps m WHERE m.owner_id = ? AND m.status = 'published' AND m.privacy = 'public'
     ORDER BY m.visits DESC`, ctx.params.id
  );
  ctx.out(200, { ok: true, maps: rows.map((r) => mapCard(r, ctx.user?.id)) });
});

// ---- play session --------------------------------------------------------
post('/api/maps/:id/join', async (ctx) => {
  const m = get('SELECT * FROM maps WHERE id = ?', ctx.params.id);
  if (!m) throw notFound('الماب غير موجود');
  if (m.status !== 'published' && m.owner_id !== ctx.user.id) throw forbidden('هذا الماب غير منشور');
  const world = safeJson(m.world, {});
  const errs = validateWorld(world);
  if (errs.length) throw badRequest('world_incomplete', 'هذا الماب غير مكتمل ولا يمكن اللعب فيه');
  // visits are counted once per distinct player per map (server side, not client claims)
  const firstVisit = !get('SELECT 1 AS x FROM map_sessions WHERE map_id = ? AND user_id = ?', m.id, ctx.user.id);
  if (firstVisit) run('UPDATE maps SET visits = visits + 1 WHERE id = ?', m.id);
  const session = startPlaySession(m.id, ctx.user.id);
  run('INSERT INTO audit_log(id,actor_id,action,target,detail,ip,created_at) VALUES(?,?,?,?,?,?,?)',
    newId('aud'), ctx.user.id, 'map.join', m.id, JSON.stringify({ sessionId: session.sessionId }), ctx.ip, nowSec());
  ctx.out(200, {
    ok: true,
    sessionId: session.sessionId,
    map: mapDetail(m, ctx.user.id),
    serverTime: nowSec(),
    economy: { miyPerMinute: 5, heartbeatSeconds: 20 },
  });
}, { auth: true });

post('/api/play/heartbeat', async (ctx) => {
  const result = heartbeat(String(ctx.body.sessionId || ''), ctx.user.id);
  ctx.out(200, { ok: true, ...result });
}, { auth: true });

post('/api/play/leave', async (ctx) => {
  const result = endPlaySession(String(ctx.body.sessionId || ''), ctx.user.id);
  ctx.out(200, { ok: true, ...result });
}, { auth: true });

// ---- game passes ---------------------------------------------------------
post('/api/maps/:id/passes', async (ctx) => {
  const m = ownMap(ctx.params.id, ctx.user);
  const name = cleanText(ctx.body.name, 50, 'اسم الألباس');
  const description = cleanText(ctx.body.description, 300, 'الوصف');
  const price = clampInt(ctx.body.price, 0, 100000, 50);
  if (name.length < 3) throw badRequest('bad_name', 'اسم الألباس قصير جداً');
  const perks = Array.isArray(ctx.body.perks) ? ctx.body.perks.slice(0, 8).map((p) => String(p).slice(0, 100)) : [];
  const id = newId('pass');
  run(
    'INSERT INTO game_passes(id,map_id,creator_id,name,description,price,image,perks,published,created_at) VALUES(?,?,?,?,?,?,?,?,1,?)',
    id, m.id, ctx.user.id, name, description, price, String(ctx.body.image || '').slice(0, 400000),
    JSON.stringify(perks), nowSec()
  );
  ctx.out(201, { ok: true, pass: { id, name, description, price, perks, image: '' } });
}, { auth: true });

post('/api/passes/:id/buy', async (ctx) => {
  const p = get('SELECT * FROM game_passes WHERE id = ? AND published = 1', ctx.params.id);
  if (!p) throw notFound('الألباس غير موجود');
  if (get('SELECT 1 AS x FROM pass_ownership WHERE pass_id = ? AND user_id = ?', p.id, ctx.user.id)) {
    throw conflict('already_owned', 'تملك هذا الألباس بالفعل');
  }
  const { credit } = await import('../core/economy.js');
  const ref = `pass:${p.id}:${ctx.user.id}`;
  const res = credit(ctx.user.id, -p.price, 'gamepass', ref, { passId: p.id, mapId: p.map_id });
  const ins = run('INSERT OR IGNORE INTO pass_ownership(user_id,pass_id,purchased_at,price_paid) VALUES(?,?,?,?)',
    ctx.user.id, p.id, nowSec(), p.price);
  if (!ins.changes) {
    // rollback if the ownership insert lost a race
    credit(ctx.user.id, p.price, 'gamepass_refund', `${ref}:refund`, { passId: p.id });
    throw conflict('already_owned', 'تملك هذا الألباس بالفعل');
  }
  const creatorShare = Math.floor(p.price * 0.7);
  if (creatorShare > 0) {
    credit(p.creator_id, creatorShare, 'gamepass_earning', `passEarn:${p.id}:${ctx.user.id}`, { passId: p.id });
    notify(p.creator_id, 'economy', `+${creatorShare} Miy`, `بيع ألباس «${p.name}»`);
  }
  notify(ctx.user.id, 'economy', 'شراء ناجح', `تم شراء ألباس «${p.name}» مقابل ${p.price} Miy`);
  ctx.out(200, { ok: true, balance: res.balance, owned: true });
}, { auth: true });

getRoute('/api/me/passes', async (ctx) => {
  const rows = all(
    `SELECT g.*, o.purchased_at FROM pass_ownership o JOIN game_passes g ON g.id = o.pass_id
     WHERE o.user_id = ? ORDER BY o.purchased_at DESC`, ctx.user.id
  );
  ctx.out(200, {
    ok: true,
    passes: rows.map((p) => ({
      id: p.id, mapId: p.map_id, name: p.name, description: p.description, price: p.price,
      perks: safeJson(p.perks, []), purchasedAt: p.purchased_at,
    })),
  });
}, { auth: true });

// ---- invitations ---------------------------------------------------------
post('/api/maps/:id/invite', async (ctx) => {
  const m = get('SELECT * FROM maps WHERE id = ?', ctx.params.id);
  if (!m) throw notFound('الماب غير موجود');
  if (m.owner_id !== ctx.user.id && m.status !== 'published') throw forbidden('لا يمكن دعوة لاعبين لهذا الماب');
  const friendId = String(ctx.body.friendId || '');
  const friend = get(
    `SELECT u.* FROM users u JOIN friendships f ON
       ((f.requester_id = ? AND f.addressee_id = u.id) OR (f.addressee_id = ? AND f.requester_id = u.id))
     WHERE u.id = ? AND f.status = 'accepted'`, ctx.user.id, ctx.user.id, friendId
  );
  if (!friend) throw forbidden('الدعوة متاحة للأصدقاء فقط');
  notify(friend.id, 'invite', 'دعوة إلى ماب', `${ctx.user.username} يدعوك إلى «${m.name}»`, { mapId: m.id });
  ctx.out(200, { ok: true });
}, { auth: true });

// deterministic share link (server-side, no fake analytics)
getRoute('/api/maps/:id/share', async (ctx) => {
  const m = get('SELECT * FROM maps WHERE id = ?', ctx.params.id);
  if (!m) throw notFound('الماب غير موجود');
  ctx.out(200, { ok: true, code: m.id, text: `العب «${m.name}» على NEXORA — nexo.ra/${m.id}` });
});
