// NEXORA backend — client contract tests.
//
// The Android client parses these payloads directly, so the shapes are pinned here:
// one of them (realtime room payloads using string ids) already broke the remote
// player rendering once.
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import net from 'node:net';
import crypto from 'node:crypto';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const PORT = 18600 + Math.floor(Math.random() * 300);
const DB = path.join(os.tmpdir(), `nexora-contract-${Date.now()}.db`);
let server;

async function api(pathname, { method = 'GET', body, token } = {}) {
  const res = await fetch(`http://127.0.0.1:${PORT}${pathname}`, {
    method,
    headers: { 'content-type': 'application/json', ...(token ? { authorization: `Bearer ${token}` } : {}) },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  return { status: res.status, json: await res.json().catch(() => null) };
}

// Minimal WebSocket client: the protocol under test is the engine's own framing
// (one masked text frame out, unmasked JSON frames in), not a browser API.
function wsConnect() {
  return new Promise((resolve, reject) => {
    const key = crypto.randomBytes(16).toString('base64');
    const sock = net.connect(PORT, '127.0.0.1', () => {
      sock.write(`GET /ws HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n`
        + `Sec-WebSocket-Key: ${key}\r\nSec-WebSocket-Version: 13\r\n\r\n`);
    });
    sock.on('error', reject);
    let buf = Buffer.alloc(0);
    let handshook = false;
    const frames = [];
    sock.on('data', (d) => {
      buf = Buffer.concat([buf, d]);
      if (!handshook) {
        const i = buf.indexOf('\r\n\r\n');
        if (i < 0) return;
        buf = buf.subarray(i + 4);
        handshook = true;
        resolve(client);
      }
      while (buf.length >= 2) {
        const short = buf[1] & 0x7f;
        let off = 2, len = short;
        if (short === 126) { len = buf.readUInt16BE(2); off = 4; }
        else if (short === 127) { len = Number(buf.readBigUInt64BE(2)); off = 10; }
        if (buf.length < off + len) break;
        try { frames.push(JSON.parse(buf.subarray(off, off + len).toString())); } catch { /* ignore */ }
        buf = buf.subarray(off + len);
      }
    });
    const client = {
      frames,
      send(obj) {
        const payload = Buffer.from(JSON.stringify(obj));
        const mask = crypto.randomBytes(4);
        let head;
        if (payload.length < 126) head = Buffer.from([0x81, 0x80 | payload.length]);
        else { head = Buffer.alloc(4); head[0] = 0x81; head[1] = 0x80 | 126; head.writeUInt16BE(payload.length, 2); }
        sock.write(Buffer.concat([head, mask, Buffer.from(payload.map((b, i) => b ^ mask[i % 4]))]));
      },
      wait(type, ms = 3000) {
        return client.waitFor((f) => f.type === type, ms);
      },
      // tick broadcasts stream continuously, so wait for the first match of a predicate
      waitFor(pred, ms = 3000) {
        const t0 = Date.now();
        return new Promise((res) => {
          (function poll() {
            const f = frames.find(pred);
            if (f) return res(f);
            if (Date.now() - t0 > ms) return res(null);
            setTimeout(poll, 15);
          })();
        });
      },
      close() { sock.destroy(); },
    };
  });
}

const uniq = Date.now().toString(36);
const mk = (n) => ({ username: `${n}${uniq}`, email: `${n}${uniq}@example.com`, password: 'Contract123!', birthDate: '2000-01-01', acceptedTerms: true });
let host, guest, mapId;

before(async () => {
  server = spawn(process.execPath, ['src/index.js'], {
    cwd: root,
    env: { ...process.env, PORT: String(PORT), DB_PATH: DB, HOST: '127.0.0.1', NEXORA_SECRET: 'c'.repeat(48) },
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  await new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error('server did not start')), 20000);
    server.stdout.on('data', (d) => { if (String(d).includes('listening')) { clearTimeout(t); res(); } });
    server.stderr.on('data', (d) => process.stderr.write(d));
  });
});

after(() => { server?.kill('SIGTERM'); });

test('registering two players and publishing a map', async () => {
  const a = await api('/api/auth/register', { method: 'POST', body: mk('host') });
  const b = await api('/api/auth/register', { method: 'POST', body: mk('guest') });
  assert.equal(a.status, 201);
  assert.equal(b.status, 201);
  host = { token: a.json.token, id: a.json.user.id };
  guest = { token: b.json.token, id: b.json.user.id };
  assert.match(host.id, /^usr_/, 'user ids are prefixed strings');
  const created = await api('/api/maps', { method: 'POST', token: host.token, body: { name: 'Contract Isle', gameType: 'adventure' } });
  assert.equal(created.status, 201);
  mapId = created.json.map.id;
  const published = await api(`/api/maps/${mapId}/publish`, { method: 'POST', token: host.token, body: {} });
  assert.equal(published.status, 200);
});

test('room payloads identify players by string id, never numbers', async () => {
  const a = await wsConnect();
  assert.ok(await a.wait('hello'), 'hello frame');
  a.send({ type: 'auth', token: host.token });
  assert.ok(await a.wait('authed'));
  a.send({ type: 'join_map', mapId, x: 1, y: 2, z: 3, heading: 0 });
  const state = await a.wait('room_state');
  assert.ok(state, 'room_state frame');
  assert.equal(state.count, 1);
  assert.equal(state.players[0].id, host.id);
  for (const key of ['username', 'x', 'y', 'z', 'heading', 'moving']) {
    assert.ok(state.players[0][key] !== undefined, `room player missing ${key}`);
  }

  const b = await wsConnect();
  await b.wait('hello');
  b.send({ type: 'auth', token: guest.token });
  await b.wait('authed');
  b.send({ type: 'join_map', mapId, x: 8, y: 2, z: 8, heading: 1 });
  const joined = await a.wait('player_joined');
  assert.equal(joined.player.id, guest.id, 'player_joined carries the joiner id');

  const tick = await a.waitFor((f) => f.type === 'room_tick' && f.players?.length === 2);
  assert.ok(tick, 'a tick carrying both players');
  assert.ok(tick.players.every((p) => typeof p.id === 'string' && p.id.startsWith('usr_')));

  // map chat reaches the other player and keeps its Arabic body intact
  a.send({ type: 'chat', channel: 'map', body: 'مرحبا من الاختبار' });
  const chat = await b.waitFor((f) => f.type === 'chat' && f.message?.body);
  assert.equal(chat.message.body, 'مرحبا من الاختبار');
  assert.equal(chat.message.sender.id, host.id);

  b.close();
  const left = await a.wait('player_left');
  assert.equal(left.userId, guest.id, 'player_left carries the string id');
  a.close();
});

test('map list and detail expose what the client renders', async () => {
  const list = await api('/api/maps?sort=popular&limit=5', { token: guest.token });
  assert.equal(list.status, 200);
  const card = list.json.maps.find((m) => m.id === mapId);
  assert.ok(card, 'published map is listed');
  for (const key of ['name', 'description', 'gameType', 'visits', 'likes', 'players', 'owner']) {
    assert.ok(card[key] !== undefined, `map card missing ${key}`);
  }
  assert.equal(card.owner.username.startsWith('host'), true);

  const detail = await api(`/api/maps/${mapId}`, { token: guest.token });
  assert.equal(detail.status, 200);
  assert.equal(detail.json.map.id, mapId);
  assert.ok(detail.json.map.ownerDetails);
  assert.ok(Array.isArray(detail.json.map.passesList));
});

test('sort=players orders by live occupancy, not by recency', async () => {
  // two more published maps; only one of them gets a live player
  const mkMap = async (name) => {
    const created = await api('/api/maps', { method: 'POST', token: host.token, body: { name, gameType: 'adventure' } });
    assert.equal(created.status, 201);
    await api(`/api/maps/${created.json.map.id}/publish`, { method: 'POST', token: host.token, body: {} });
    return created.json.map.id;
  };
  const busy = await mkMap('Busy Isle');
  await mkMap('Empty Isle');

  const ws = await wsConnect();
  await ws.wait('hello');
  ws.send({ type: 'auth', token: guest.token });
  await ws.wait('authed');
  ws.send({ type: 'join_map', mapId: busy, x: 0, y: 0, z: 0, heading: 0 });
  assert.ok(await ws.wait('room_state'));

  const list = await api('/api/maps?sort=players&limit=5', { token: guest.token });
  assert.equal(list.status, 200);
  assert.equal(list.json.maps[0].id, busy, 'the occupied map comes first');
  assert.equal(list.json.maps[0].players, 1);
  ws.close();
});
