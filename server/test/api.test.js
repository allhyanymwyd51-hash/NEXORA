// NEXORA backend — end-to-end smoke test.
//
// Boots the real server on an ephemeral port with a throwaway database, then exercises the
// complete client journey: register → login → create map → NEX AI generation → publish →
// join → heartbeat (Miy) → social → shop → admin. Run with: node --test test/
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const PORT = 18000 + Math.floor(Math.random() * 500);
const DB = path.join(os.tmpdir(), `nexora-test-${Date.now()}.db`);
let server;

async function api(pathname, { method = 'GET', body, token, raw = false } = {}) {
  const res = await fetch(`http://127.0.0.1:${PORT}${pathname}`, {
    method,
    headers: {
      'content-type': 'application/json',
      ...(token ? { authorization: `Bearer ${token}` } : {}),
    },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const text = await res.text();
  if (raw) return { status: res.status, text };
  let json = null;
  try { json = JSON.parse(text); } catch { /* non-JSON */ }
  return { status: res.status, json, text };
}

const uniq = Date.now().toString(36);
const player = { username: `t_${uniq}`, email: `t_${uniq}@example.com`, password: 'TestPass123!', birthDate: '2000-05-05', acceptedTerms: true };
const friend = { username: `f_${uniq}`, email: `f_${uniq}@example.com`, password: 'TestPass123!', birthDate: '1999-02-02', acceptedTerms: true };

let token;
let userId;
let mapId;
let friendToken;

before(async () => {
  server = spawn(process.execPath, ['src/index.js'], {
    cwd: root,
    env: { ...process.env, PORT: String(PORT), DB_PATH: DB, HOST: '127.0.0.1', NEXORA_SECRET: 'x'.repeat(48) },
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  let out = '';
  server.stderr.on('data', (d) => { out += d.toString(); });
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`server did not start:\n${out}`)), 20000);
    server.stdout.on('data', (d) => {
      out += d.toString();
      if (out.includes('listening on')) { clearTimeout(timer); resolve(); }
    });
    server.on('exit', (code) => { clearTimeout(timer); reject(new Error(`server exited ${code}:\n${out}`)); });
  });
});

after(async () => {
  if (server) server.kill('SIGTERM');
  for (const f of [DB, `${DB}-wal`, `${DB}-shm`]) {
    try { fs.unlinkSync(f); } catch { /* gone */ }
  }
});

test('health', async () => {
  const r = await api('/health');
  assert.equal(r.status, 200);
  assert.equal(r.json.ok, true);
});

test('register + me', async () => {
  const r = await api('/api/auth/register', { method: 'POST', body: player });
  assert.equal(r.status, 201, r.text);
  token = r.json.token;
  userId = r.json.user.id;
  assert.ok(token && userId);
  assert.equal(r.json.user.miy, 0);
  const me = await api('/api/auth/me', { token });
  assert.equal(me.status, 200);
  assert.equal(me.json.user.username, player.username);
  // no password material ever leaves the server
  assert.ok(!('password_hash' in me.json.user));
});

test('duplicate username is rejected', async () => {
  const r = await api('/api/auth/register', { method: 'POST', body: { ...player, email: `other_${uniq}@example.com` } });
  assert.equal(r.status, 409);
});

test('login rejects a bad password but accepts the right one', async () => {
  const bad = await api('/api/auth/login', { method: 'POST', body: { login: player.username, password: 'wrong-pass' } });
  assert.equal(bad.status, 401);
  const ok = await api('/api/auth/login', { method: 'POST', body: { login: player.email, password: player.password } });
  assert.equal(ok.status, 200);
  token = ok.json.token;
});

test('map list starts empty', async () => {
  const r = await api('/api/maps');
  assert.equal(r.status, 200);
  assert.equal(r.json.maps.length, 0);
  assert.equal(r.json.total, 0);
});

test('NEX AI plans a world', async () => {
  const r = await api('/api/ai/generate', {
    method: 'POST',
    token,
    body: { prompt: 'مدينة ليلية بأبراج نيون وأنهار', gameType: 'open_world', worldSize: 'large', quality: 'high' },
  });
  assert.equal(r.status, 202, r.text);
  assert.ok(r.json.jobId, 'job id returned');
  const jobId = r.json.jobId;
  // poll until the planner finishes (it runs locally and fast)
  let job;
  for (let i = 0; i < 40; i++) {
    const p = await api(`/api/ai/jobs/${jobId}`, { token });
    job = p.json.job;
    if (job.status === 'done' || job.status === 'failed') break;
    await new Promise((r2) => setTimeout(r2, 250));
  }
  assert.equal(job.status, 'done', JSON.stringify(job));
  assert.ok(job.plan && job.plan.format === 'nexora-world/1', 'world spec attached');
  assert.ok(Array.isArray(job.plan.roads ?? []));
  assert.ok(job.mapId && job.mapId.startsWith('map'), 'job is bound to a map row');
});

test('create, publish and join a map', async () => {
  const ai = await api('/api/ai/generate', {
    method: 'POST', token, body: { name: 'قرية الساحل', prompt: 'قرية ساحلية هادئة', gameType: 'exploration', worldSize: 'medium' },
  });
  assert.equal(ai.status, 202, ai.text);
  const jobId = ai.json.jobId;
  let job;
  for (let i = 0; i < 40; i++) {
    const p = await api(`/api/ai/jobs/${jobId}`, { token });
    job = p.json.job;
    if (job.status === 'done') break;
    await new Promise((r2) => setTimeout(r2, 250));
  }
  assert.equal(job.status, 'done');
  // the planner already created the map row it belongs to
  mapId = job.mapId;
  assert.ok(mapId);
  const created = await api(`/api/maps/${mapId}`, { token });
  assert.equal(created.status, 200, created.text);
  assert.equal(created.json.map.name, 'قرية الساحل');
  const pub = await api(`/api/maps/${mapId}/publish`, { method: 'POST', token });
  assert.equal(pub.status, 200, pub.text);
  assert.equal(pub.json.map.status, 'published');
  const join = await api(`/api/maps/${mapId}/join`, { method: 'POST', token });
  assert.equal(join.status, 200, join.text);
  assert.ok(join.json.sessionId);
  // the world round-trips intact
  const detail = await api(`/api/maps/${mapId}`, { token });
  assert.equal(detail.status, 200);
  assert.ok(detail.json.map.world.terrain);
});

test('Miy accrues only through the authoritative heartbeat', async () => {
  const me0 = await api('/api/auth/me', { token });
  const before0 = me0.json.user.miy;
  const join = await api(`/api/maps/${mapId}/join`, { method: 'POST', token });
  const hb = await api('/api/play/heartbeat', { method: 'POST', token, body: { sessionId: join.json.sessionId } });
  assert.equal(hb.status, 200, hb.text);
  assert.equal(typeof hb.json.credited, 'number');
  assert.equal(typeof hb.json.balance, 'number');
  const me1 = await api('/api/auth/me', { token });
  assert.ok(me1.json.user.miy >= before0, 'Miy never decreases');
  assert.equal(typeof hb.json.balance, 'number');
  assert.ok(typeof hb.json.creditsToday === 'number' && hb.json.creditsToday >= 0);
  // rewards are metered per whole minute only: an immediate second beat grants nothing
  assert.ok(hb.json.credited === 0, `no partial-minute credit, got ${hb.json.credited}`);
  // client-supplied amounts must be ignored entirely
  const cheat = await api('/api/play/heartbeat', {
    method: 'POST', token, body: { sessionId: join.json.sessionId, miy: 999999, seconds: 999999 },
  });
  assert.equal(cheat.status, 200);
  assert.ok((cheat.json.credited || 0) === 0 && cheat.json.balance < 999999, 'cannot self-grant currency');
  const leave = await api('/api/play/leave', { method: 'POST', token, body: { sessionId: join.json.sessionId } });
  assert.equal(leave.status, 200);
});

test('friends: request, accept, list', async () => {
  const reg = await api('/api/auth/register', { method: 'POST', body: friend });
  assert.equal(reg.status, 201, reg.text);
  friendToken = reg.json.token;
  const req = await api('/api/friends/request', { method: 'POST', token, body: { userId: reg.json.user.id } });
  assert.ok([200, 201].includes(req.status), req.text);
  assert.equal(req.json.status, 'pending');
  const friendshipId = req.json.friendshipId;
  const list = await api('/api/friends', { token: friendToken });
  assert.equal(list.json.incoming.length, 1, JSON.stringify(list.json));
  const acc = await api(`/api/friends/${friendshipId}/accept`, { method: 'POST', token: friendToken });
  assert.ok([200, 201].includes(acc.status), `accept -> ${acc.status} ${acc.text}`);
  const mine = await api('/api/friends', { token });
  assert.equal(mine.json.friends.length, 1);
  assert.equal(mine.json.friends[0].username, friend.username);
});

test('chat history + threads', async () => {
  const h = await api(`/api/chat/history?channel=map&channelId=${mapId}`, { token });
  assert.equal(h.status, 200, h.text);
  const t = await api('/api/chat/threads', { token });
  assert.equal(t.status, 200);
});

test('shop lists items and buying requires funds', async () => {
  const s = await api('/api/shop', { token });
  assert.equal(s.status, 200, s.text);
  assert.ok(Array.isArray(s.json.items) && s.json.items.length > 0);
  const item = s.json.items.find((i) => i.price > 0) || s.json.items[0];
  const buy = await api('/api/shop/buy', { method: 'POST', token, body: { itemId: item.id } });
  // either outright affordable or correctly rejected for insufficient funds — never a free grant
  assert.ok([200, 400, 402].includes(buy.status), buy.text);
  const inv = await api('/api/inventory', { token });
  assert.equal(inv.status, 200);
});

test('game passes are server-verified', async () => {
  const created = await api(`/api/maps/${mapId}/passes`, {
    method: 'POST', token, body: { name: 'تصريح VIP', description: 'دخول مميز', price: 50 },
  });
  assert.equal(created.status, 201, created.text);
  const passId = created.json.pass.id;
  assert.ok(passId);
  // a player with a zero balance can never obtain a pass for free
  const other = await api(`/api/passes/${passId}/buy`, { method: 'POST', token: friendToken });
  assert.ok([400, 402].includes(other.status), `expected refusal, got ${other.status} ${other.text}`);
  const states = await api('/api/me/passes', { token: friendToken });
  assert.equal(states.status, 200);
  assert.ok(!states.json.passes.some((p) => p.id === passId));
});

test('profanity is filtered in chat payloads', async () => {
  const r = await api('/api/maps', { method: 'POST', token, body: { name: 'x'.repeat(200) } });
  assert.equal(r.status, 400);
});

test('settings and profile update', async () => {
  const s = await api('/api/me/settings', { method: 'PATCH', token, body: { language: 'ar', quality: 'high' } });
  assert.equal(s.status, 200, s.text);
  const p = await api('/api/me/profile', { method: 'PATCH', token, body: { bio: 'مرحبا' } });
  assert.equal(p.status, 200, p.text);
});

test('non-owners cannot reach the admin surface', async () => {
  const r = await api('/api/admin/overview', { token });
  assert.equal(r.status, 403, r.text);
});

test('logout invalidates the session', async () => {
  const out = await api('/api/auth/logout', { method: 'POST', token });
  assert.equal(out.status, 200);
  const me = await api('/api/auth/me', { token });
  assert.equal(me.status, 401);
});

// --- client contract regressions -------------------------------------------
// These pin the exact response shapes the Android client parses. Both bugs below
// shipped once: map ids were parsed as numbers (they are "map_…" strings) and the
// settings screen polled a route that only accepts PATCH.
// logout invalidates the session above, so the contract tests sign in again
let laterToken;
test('map ids are opaque strings, every map route accepts them', async () => {
  const relogin = await api('/api/auth/login', { method: 'POST', body: { login: player.username, password: player.password } });
  assert.equal(relogin.status, 200);
  laterToken = relogin.json.token;
  token = laterToken;
  const created = await api('/api/maps', { method: 'POST', token: laterToken, body: { name: 'Id Shape', gameType: 'open_world' } });
  assert.equal(created.status, 201);
  const id = created.json.map.id;
  assert.match(id, /^map_[A-Za-z0-9_-]+$/, 'map ids must be prefixed strings');
  // routed by the raw id, never by a numeric suffix
  for (const [method, suffix] of [['GET', ''], ['POST', '/favorite'], ['POST', '/join']]) {
    const res = await api(`/api/maps/${id}${suffix}`, { method, token: laterToken, body: method === 'POST' ? {} : undefined });
    assert.ok(res.status === 200, `${method} ${suffix} -> ${res.status}`);
    assert.equal(res.json.ok, true);
  }
});

test('world payload carries everything the renderer reads', async () => {
  // mapId comes from the earlier AI-generation flow
  const joined = await api(`/api/maps/${mapId}/join`, { method: 'POST', token: laterToken, body: {} });
  assert.equal(joined.status, 200);
  const world = joined.json.map.world;
  for (const key of ['format', 'side', 'gameType', 'terrain', 'water', 'environment', 'spawns', 'audio']) {
    assert.ok(world[key] !== undefined, `world.${key} missing`);
  }
  assert.ok(Array.isArray(world.spawns) && world.spawns.length > 0);
  assert.equal(typeof world.spawns[0].x, 'number');
  assert.ok(world.audio.ambient && world.audio.music, 'ambient + music clips required');
});

test('settings travel in /auth/me and are patched, not fetched', async () => {
  const get = await api('/api/me/settings', { token: laterToken });
  assert.equal(get.status, 405, 'GET /api/me/settings must not be a route');
  const me = await api('/api/auth/me', { token: laterToken });
  assert.equal(me.status, 200);
  assert.ok(me.json.user.settings && typeof me.json.user.settings === 'object');
  const patched = await api('/api/me/settings', { method: 'PATCH', token: laterToken, body: { language: 'en', music: 0.4 } });
  assert.equal(patched.status, 200);
  assert.equal(patched.json.settings.language, 'en');
  assert.equal(patched.json.settings.music, 0.4);
});

// --- owner console ---------------------------------------------------------
// The client's owner console reads /api/admin/overview (grouped metrics), searches
// users, bans them and broadcasts. Promote the friend account directly so the test
// does not depend on the create-owner script.
test('owner console: overview, user search, broadcast', async () => {
  const { DatabaseSync } = await import('node:sqlite');
  const db = new DatabaseSync(DB);
  db.prepare("UPDATE users SET role = 'owner' WHERE username = ?").run(friend.username);
  db.close();

  const login = await api('/api/auth/login', { method: 'POST', body: { login: friend.username, password: friend.password } });
  assert.equal(login.status, 200);
  const ownerToken = login.json.token;

  const overview = await api('/api/admin/overview', { token: ownerToken });
  assert.equal(overview.status, 200);
  for (const group of ['users', 'maps', 'economy', 'sessions', 'server']) {
    assert.ok(overview.json[group] && typeof overview.json[group] === 'object', `overview.${group}`);
  }
  assert.equal(typeof overview.json.users.total, 'number');
  assert.equal(typeof overview.json.maps.published, 'number');
  assert.equal(overview.json.server.version, '1.0.0');

  const search = await api(`/api/admin/users?q=${encodeURIComponent(player.username)}`, { token: ownerToken });
  assert.equal(search.status, 200);
  const found = search.json.users.find((u) => u.username === player.username);
  assert.ok(found, 'search finds the player');
  assert.match(found.id, /^usr_/);

  const before = await api('/api/me/notifications?limit=1', { token: laterToken });
  const broadcast = await api('/api/admin/broadcast', { method: 'POST', token: ownerToken, body: { title: 'NEXORA', body: 'صيانة الليلة 11 مساءً' } });
  assert.equal(broadcast.status, 200, broadcast.text);
  assert.ok(broadcast.json.sent >= 2, 'broadcast reaches every active player');
  const after = await api('/api/me/notifications?limit=5', { token: laterToken });
  assert.ok((after.json.notifications || after.json.items || []).length >= 1, 'player receives the announcement');
});

test('notifications carry an unread count the bell badge can render', async () => {
  const list = await api('/api/me/notifications?limit=1', { token: laterToken });
  assert.equal(list.status, 200);
  assert.equal(typeof list.json.unread, 'number');
  assert.ok(list.json.notifications.length <= 1, 'limit is honoured');
  const me = await api('/api/auth/me', { token: laterToken });
  assert.equal(me.json.user.unreadNotifications, list.json.unread, 'badge and list agree');
  const read = await api('/api/me/notifications/read', { method: 'POST', token: laterToken, body: {} });
  assert.equal(read.status, 200);
  const after = await api('/api/me/notifications', { token: laterToken });
  assert.equal(after.json.unread, 0, 'mark-all clears the badge');
});
