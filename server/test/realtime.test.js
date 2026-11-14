// NEXORA realtime — WebSocket protocol test against the real server.
//
// Covers: token auth, map join/room_state, player_joined, 5 Hz room_tick position sync,
// in-map chat broadcast, DM routing, presence and voice peer signalling.
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const PORT = 18600 + Math.floor(Math.random() * 300);
const DB = path.join(os.tmpdir(), `nexora-ws-${Date.now()}.db`);
const uniq = Date.now().toString(36);
let server;
let mapId;
const tokens = {};
const ids = {};

const http = async (pathname, { method = 'GET', body, token } = {}) => {
  const res = await fetch(`http://127.0.0.1:${PORT}${pathname}`, {
    method,
    headers: { 'content-type': 'application/json', ...(token ? { authorization: `Bearer ${token}` } : {}) },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const text = await res.text();
  return { status: res.status, json: text ? JSON.parse(text) : null, text };
};

// A tiny promise-based socket that keeps every frame it receives.
function openSocket(name) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${PORT}/ws`);
    const frames = [];
    const waiters = [];
    ws.onmessage = (ev) => {
      let msg;
      try { msg = JSON.parse(typeof ev.data === 'string' ? ev.data : Buffer.from(ev.data).toString('utf8')); }
      catch { return; }
      frames.push(msg);
      for (let i = waiters.length - 1; i >= 0; i--) {
        if (waiters[i].match(msg)) { waiters[i].resolve(msg); waiters.splice(i, 1); }
      }
    };
    ws.onerror = (e) => reject(new Error(`${name} socket error`));
    ws.onopen = () => resolve({
      name, ws, frames,
      send(obj) { ws.send(JSON.stringify(obj)); },
      waitFor(match, timeoutMs = 6000) {
        const existing = frames.find(match);
        if (existing) return Promise.resolve(existing);
        return new Promise((res, rej) => {
          const w = { match, resolve: res };
          waiters.push(w);
          setTimeout(() => {
            const i = waiters.indexOf(w);
            if (i >= 0) { waiters.splice(i, 1); rej(new Error(`${name}: timeout waiting for frame`)); }
          }, timeoutMs);
        });
      },
      close() { try { ws.close(); } catch { /* gone */ } },
    });
  });
}

before(async () => {
  server = spawn(process.execPath, ['src/index.js'], {
    cwd: root,
    env: { ...process.env, PORT: String(PORT), DB_PATH: DB, HOST: '127.0.0.1', NEXORA_SECRET: 'y'.repeat(48) },
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  let out = '';
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`server did not start:\n${out}`)), 20000);
    server.stdout.on('data', (d) => {
      out += d.toString();
      if (out.includes('listening on')) { clearTimeout(timer); resolve(); }
    });
    server.stderr.on('data', (d) => { out += d.toString(); });
    server.on('exit', (code) => { clearTimeout(timer); reject(new Error(`server exited ${code}:\n${out}`)); });
  });
  // two accounts + one published map
  for (const who of ['a', 'b']) {
    const reg = await http('/api/auth/register', {
      method: 'POST',
      body: { username: `w${who}_${uniq}`, email: `w${who}_${uniq}@example.com`, password: 'TestPass123!', birthDate: '2000-01-01', acceptedTerms: true },
    });
    assert.equal(reg.status, 201, reg.text);
    tokens[who] = reg.json.token;
    ids[who] = reg.json.user.id;
  }
  const ai = await http('/api/ai/generate', {
    method: 'POST', token: tokens.a,
    body: { name: 'ماب الشبكة', prompt: 'سهول واسعة مع نهر وبحيرة صغيرة', gameType: 'open_world', worldSize: 'small' },
  });
  assert.equal(ai.status, 202, ai.text);
  mapId = ai.json.mapId;
  let job;
  for (let i = 0; i < 40; i++) {
    job = (await http(`/api/ai/jobs/${ai.json.jobId}`, { token: tokens.a })).json.job;
    if (job.status === 'done') break;
    await new Promise((r) => setTimeout(r, 250));
  }
  assert.equal(job.status, 'done');
  const pub = await http(`/api/maps/${mapId}/publish`, { method: 'POST', token: tokens.a });
  assert.equal(pub.status, 200, pub.text);
});

after(async () => {
  if (server) server.kill('SIGTERM');
  for (const f of [DB, `${DB}-wal`, `${DB}-shm`]) { try { fs.unlinkSync(f); } catch { /* gone */ } }
});

test('bad token is rejected', async () => {
  const s = await openSocket('bad');
  s.send({ type: 'auth', token: 'not-a-real-token' });
  const err = await s.waitFor((m) => m.type === 'error');
  assert.equal(err.error, 'bad_token');
  s.close();
});

test('auth, join map, see each other, sync movement, chat', async () => {
  const a = await openSocket('a');
  const b = await openSocket('b');
  a.send({ type: 'auth', token: tokens.a });
  b.send({ type: 'auth', token: tokens.b });
  const authedA = await a.waitFor((m) => m.type === 'authed');
  assert.equal(authedA.user.id, ids.a);
  await b.waitFor((m) => m.type === 'authed');

  a.send({ type: 'join_map', mapId, x: 0, y: 0, z: 0, heading: 0 });
  const state = await a.waitFor((m) => m.type === 'room_state');
  assert.equal(state.mapId, mapId);
  assert.ok(Array.isArray(state.players));

  b.send({ type: 'join_map', mapId, x: 3, y: 0, z: -2, heading: 1 });
  const joined = await a.waitFor((m) => m.type === 'player_joined');
  assert.equal(joined.player.id, ids.b);
  assert.equal(joined.player.username, `wb_${uniq}`);

  // a move from B must reach A through the server's room tick (server is the authority)
  b.send({ type: 'move', x: 12.5, y: 0.25, z: -7.75, heading: 2.1, moving: true });
  const tick = await a.waitFor(
    (m) => m.type === 'room_tick' && m.players.some((p) => p.id === ids.b && Math.abs(p.x - 12.5) < 0.01),
    8000,
  );
  const me = tick.players.find((p) => p.id === ids.b);
  assert.equal(me.z, -7.75);
  assert.equal(me.moving, true);

  // chat goes to the whole room, including the sender
  a.send({ type: 'chat', body: 'مرحبا بالجميع', channel: 'map' });
  const chatA = await a.waitFor((m) => m.type === 'chat' && m.message.body === 'مرحبا بالجميع');
  const chatB = await b.waitFor((m) => m.type === 'chat' && m.message.body === 'مرحبا بالجميع');
  assert.equal(chatA.message.sender.id, ids.a);
  assert.equal(chatB.message.sender.username, `wa_${uniq}`);

  // the message is persisted and readable over HTTP
  const hist = await http(`/api/chat/history?channel=map&channelId=${mapId}`, { token: tokens.b });
  assert.equal(hist.status, 200, hist.text);
  const stored = hist.json.messages.filter((m) => m.body.includes('الجميع'));
  assert.equal(stored.length, 1, JSON.stringify(hist.json.messages));
  assert.equal(stored[0].sender.id, ids.a);
  assert.equal(stored[0].sender.username, `wa_${uniq}`);

  // leaving is broadcast
  b.send({ type: 'leave_map' });
  const left = await a.waitFor((m) => m.type === 'player_left');
  assert.equal(left.userId, ids.b);
  a.close();
  b.close();
});

test('voice signalling is relayed only inside the map room', async () => {
  const a = await openSocket('va');
  const b = await openSocket('vb');
  a.send({ type: 'auth', token: tokens.a });
  b.send({ type: 'auth', token: tokens.b });
  await a.waitFor((m) => m.type === 'authed');
  await b.waitFor((m) => m.type === 'authed');
  a.send({ type: 'join_map', mapId, x: 0, y: 0, z: 0, heading: 0 });
  b.send({ type: 'join_map', mapId, x: 1, y: 0, z: 1, heading: 0 });
  await a.waitFor((m) => m.type === 'room_state');
  await b.waitFor((m) => m.type === 'room_state');

  b.send({ type: 'voice_join' });
  const available = await a.waitFor((m) => m.type === 'voice_peer_available');
  assert.equal(available.userId, ids.b);

  a.send({ type: 'voice_join' });
  const peers = await a.waitFor((m) => m.type === 'voice_peers');
  assert.ok(peers.peers.includes(ids.b));

  b.send({ type: 'voice_signal', to: ids.a, signal: { sdp: 'answer', kind: 'offer' } });
  const sig = await a.waitFor((m) => m.type === 'voice_signal');
  assert.equal(sig.from, ids.b);
  assert.equal(sig.signal.kind, 'offer');

  b.send({ type: 'speaking', on: true });
  const speaking = await a.waitFor(
    (m) => m.type === 'room_tick' && m.players.some((p) => p.id === ids.b && p.speaking), 8000,
  );
  assert.ok(speaking);
  a.close();
  b.close();
});

test('direct messages reach only the addressee', async () => {
  const a = await openSocket('dma');
  const b = await openSocket('dmb');
  a.send({ type: 'auth', token: tokens.a });
  b.send({ type: 'auth', token: tokens.b });
  await a.waitFor((m) => m.type === 'authed');
  await b.waitFor((m) => m.type === 'authed');
  a.send({ type: 'chat', channel: 'dm', to: ids.b, body: 'رسالة خاصة' });
  const dm = await b.waitFor((m) => m.type === 'chat' && m.message.channel === 'dm');
  assert.equal(dm.message.body, 'رسالة خاصة');
  a.close();
  b.close();
});
