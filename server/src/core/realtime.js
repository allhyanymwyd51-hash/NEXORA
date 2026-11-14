// Realtime hub: WebSocket protocol (chat, presence, map rooms, voice signalling, invites).
//
// Implements RFC 6455 directly on node:http upgrade (no dependencies): frames, ping/pong,
// fragmentation-free client frames, close handshake, per-connection heartbeat.
import crypto from 'node:crypto';
import { get, run } from './db.js';
import { newId } from './security.js';
import { sanitizeChat, containsProfanity } from './moderation.js';
import { nowSec } from './util.js';
import { userFromToken } from './auth.js';

const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

// connection registry
const sockets = new Map(); // wsId -> conn
const byUser = new Map();  // userId -> Set<wsId>

export function isOnline(userId) {
  return byUser.has(userId) && byUser.get(userId).size > 0;
}

export function onlineFriends(userId) {
  return friendsOf(userId).filter((id) => isOnline(id));
}

export function friendIdsOf(userId) {
  return friendsOf(userId);
}

function sendFrame(socket, opcode, payload) {
  const len = payload.length;
  let header;
  if (len < 126) {
    header = Buffer.from([0x80 | opcode, len]);
  } else if (len < 65536) {
    header = Buffer.alloc(4);
    header[0] = 0x80 | opcode; header[1] = 126; header.writeUInt16BE(len, 2);
  } else {
    header = Buffer.alloc(10);
    header[0] = 0x80 | opcode; header[1] = 127;
    header.writeBigUInt64BE(BigInt(len), 2);
  }
  try { socket.write(Buffer.concat([header, payload])); } catch { /* closed */ }
}

function sendJSON(conn, obj) {
  if (!conn || conn.closed) return;
  sendFrame(conn.socket, 0x1, Buffer.from(JSON.stringify(obj), 'utf8'));
}

function sendToUser(userId, obj) {
  const set = byUser.get(userId);
  if (!set) return false;
  for (const id of set) sendJSON(sockets.get(id), obj);
  return true;
}

export function pushNotification(userId, payload) {
  sendToUser(userId, { type: 'notification', notification: payload });
}

// ---- map rooms ------------------------------------------------------------
const rooms = new Map(); // mapId -> { members: Map<userId, memberInfo>, state: {...} }

function ensureRoom(mapId) {
  let r = rooms.get(mapId);
  if (!r) {
    r = { mapId, members: new Map(), createdAt: nowSec() };
    rooms.set(mapId, r);
  }
  return r;
}

export function roomState(mapId) {
  const r = rooms.get(mapId);
  if (!r) return { mapId, count: 0, players: [] };
  return {
    mapId,
    count: r.members.size,
    players: [...r.members.values()].map((p) => ({
      id: p.userId, username: p.username, avatar: p.avatar, x: round2(p.x), y: round2(p.y), z: round2(p.z),
      heading: round2(p.heading), moving: !!p.moving, speaking: !!p.speaking,
    })),
  };
}

const round2 = (v) => Math.round((Number(v) || 0) * 100) / 100;

function broadcastRoom(mapId, obj, exceptUserId) {
  const r = rooms.get(mapId);
  if (!r) return;
  for (const [uid] of r.members) {
    if (uid === exceptUserId) continue;
    sendToUser(uid, obj);
  }
}

function friendsOf(userId) {
  const rows = getDbAll(
    `SELECT requester_id, addressee_id FROM friendships WHERE status = 'accepted' AND (requester_id = ? OR addressee_id = ?)`,
    userId, userId
  );
  return rows.map((r) => (r.requester_id === userId ? r.addressee_id : r.requester_id));
}

function getDbAll(sql, ...params) {
  const d = dbAllRef;
  return d ? d(sql, ...params) : [];
}
let dbAllRef = null;
export function bindDb(allFn) { dbAllRef = allFn; }

function broadcastPresence(userId, online) {
  const fs = friendsOf(userId);
  for (const f of fs) sendToUser(f, { type: 'presence', userId, online });
}

// ---- protocol -------------------------------------------------------------
const handlers = {
  auth(conn, msg) {
    if (!msg.token) return sendJSON(conn, { type: 'error', error: 'no_token' });
    const u = userFromToken(String(msg.token));
    if (!u || u.banned) return sendJSON(conn, { type: 'error', error: 'bad_token' });
    if (conn.userId && conn.userId !== u.id) detach(conn);
    conn.userId = u.id;
    conn.username = u.username;
    conn.avatar = u.avatar || '';
    let set = byUser.get(u.id);
    if (!set) { set = new Set(); byUser.set(u.id, set); }
    set.add(conn.id);
    sendJSON(conn, { type: 'authed', user: { id: u.id, username: u.username, avatar: conn.avatar } });
    broadcastPresence(u.id, true);
    // tell this client which of its friends are online
    const friends = friendsOf(u.id);
    sendJSON(conn, { type: 'presence_list', online: friends.filter(isOnline) });
  },

  join_map(conn, msg) {
    if (!conn.userId) return;
    const mapId = String(msg.mapId || '').slice(0, 80);
    const map = get('SELECT * FROM maps WHERE id = ?', mapId);
    if (!map) return sendJSON(conn, { type: 'error', error: 'map_not_found' });
    if (map.status !== 'published' && map.owner_id !== conn.userId) {
      return sendJSON(conn, { type: 'error', error: 'map_private' });
    }
    // leave previous room
    for (const [id, r] of rooms) {
      if (r.members.has(conn.userId)) leaveRoom(id, conn, false);
    }
    const room = ensureRoom(mapId);
    if (room.members.size >= map.max_players && !room.members.has(conn.userId)) {
      return sendJSON(conn, { type: 'error', error: 'room_full', message: 'الماب ممتلئ' });
    }
    room.members.set(conn.userId, {
      userId: conn.userId, username: conn.username, avatar: conn.avatar,
      x: msg.x ?? 0, y: msg.y ?? 0, z: msg.z ?? 0, heading: msg.heading ?? 0,
      moving: false, speaking: false, joinedAt: nowSec(),
    });
    conn.mapId = mapId;
    sendJSON(conn, { type: 'room_state', ...roomState(mapId) });
    broadcastRoom(mapId, {
      type: 'player_joined',
      player: roomState(mapId).players.find((p) => p.id === conn.userId),
    }, conn.userId);
  },

  leave_map(conn) {
    if (conn.mapId) leaveRoom(conn.mapId, conn, true);
  },

  // 10 Hz positional update from clients; server keeps the authoritative shared state
  move(conn, msg) {
    if (!conn.mapId) return;
    const r = rooms.get(conn.mapId);
    const p = r?.members.get(conn.userId);
    if (!p) return;
    p.x = clampCoord(msg.x); p.y = clampCoord(msg.y); p.z = clampCoord(msg.z);
    p.heading = Number(msg.heading) || 0;
    p.moving = !!msg.moving;
    // state deltas are broadcast at 5 Hz by the room ticker (below) to save bandwidth
  },

  chat(conn, msg) {
    if (!conn.userId) return;
    const body = sanitizeChat(msg.body, 400);
    if (!body) return;
    const channel = msg.channel === 'dm' ? 'dm' : 'map';
    let channelId = '';
    if (channel === 'map') {
      if (!conn.mapId) return;
      channelId = conn.mapId;
    } else {
      const target = String(msg.to || '');
      if (!target) return;
      channelId = [conn.userId, target].sort().join(':');
    }
    const id = newId('msg');
    const at = nowSec();
    run('INSERT INTO messages(id,channel,channel_id,sender_id,body,created_at) VALUES(?,?,?,?,?,?)',
      id, channel, channelId, conn.userId, body, at);
    const payload = {
      type: 'chat', message: {
        id, body, createdAt: at, channel, channelId,
        sender: { id: conn.userId, username: conn.username, avatar: conn.avatar },
      },
    };
    if (channel === 'map') broadcastRoom(conn.mapId, payload);
    else {
      sendJSON(conn, payload);
      sendToUser(channelId.split(':').find((x) => x !== conn.userId), payload);
    }
    // moderation signal: reported for review when the filter had to mask content
    if (containsProfanity(msg.body)) {
      run('INSERT INTO reports(id,reporter_id,target_type,target_id,reason,details,status,created_at) VALUES(?,?,?,?,?,?,?,?)',
        newId('rep'), conn.userId, 'message', id, 'auto_filter', 'رسالة تم تصفية كلماتها آلياً', 'open', at);
    }
  },

  // ---- voice signalling (WebRTC-style peer signalling relayed by the server) ----
  // Media itself never touches this server; only SDP/ICE offers are relayed inside the map room.
  voice_join(conn) {
    if (!conn.mapId) return sendJSON(conn, { type: 'error', error: 'not_in_map' });
    conn.voice = true;
    broadcastRoom(conn.mapId, { type: 'voice_peer_available', userId: conn.userId }, conn.userId);
    const peers = [...(rooms.get(conn.mapId)?.members.keys() || [])].filter((u) => u !== conn.userId);
    sendJSON(conn, { type: 'voice_peers', peers });
  },
  voice_signal(conn, msg) {
    if (!conn.mapId) return;
    const target = String(msg.to || '');
    if (!target) return;
    sendToUser(target, { type: 'voice_signal', from: conn.userId, signal: msg.signal });
  },
  voice_leave(conn) {
    conn.voice = false;
    if (conn.mapId) broadcastRoom(conn.mapId, { type: 'voice_peer_left', userId: conn.userId }, conn.userId);
  },
  speaking(conn, msg) {
    if (!conn.mapId) return;
    const r = rooms.get(conn.mapId);
    const p = r?.members.get(conn.userId);
    if (p) p.speaking = !!msg.on;
  },

  // ---- friend invite to a map ----
  invite(conn, msg) {
    const friendId = String(msg.friendId || '');
    const map = get('SELECT * FROM maps WHERE id = ?', String(msg.mapId || ''));
    if (!map) return sendJSON(conn, { type: 'error', error: 'map_not_found' });
    const isFriend = getDbAll(
      `SELECT 1 AS x FROM friendships WHERE status = 'accepted' AND
       ((requester_id = ? AND addressee_id = ?) OR (requester_id = ? AND addressee_id = ?))`,
      conn.userId, friendId, friendId, conn.userId
    ).length > 0;
    if (!isFriend) return sendJSON(conn, { type: 'error', error: 'not_friend' });
    const nid = newId('ntf');
    run('INSERT INTO notifications(id,user_id,type,title,body,data,created_at) VALUES(?,?,?,?,?,?,?)',
      nid, friendId, 'invite', 'دعوة إلى ماب', `${conn.username} يدعوك إلى «${map.name}»`,
      JSON.stringify({ mapId: map.id }), nowSec());
    const delivered = sendToUser(friendId, {
      type: 'notification',
      notification: {
        id: nid, type: 'invite', title: 'دعوة إلى ماب',
        body: `${conn.username} يدعوك إلى «${map.name}»`, data: { mapId: map.id }, createdAt: nowSec(),
      },
    });
    sendJSON(conn, { type: 'invite_sent', delivered });
  },

  ping(conn) { sendJSON(conn, { type: 'pong', t: Date.now() }); },
};

function clampCoord(v) {
  const n = Number(v) || 0;
  return Math.max(-5000, Math.min(5000, n));
}

function leaveRoom(mapId, conn, notifySelf) {
  const r = rooms.get(mapId);
  if (!r) return;
  r.members.delete(conn.userId);
  broadcastRoom(mapId, { type: 'player_left', userId: conn.userId });
  if (notifySelf) sendJSON(conn, { type: 'left_map', mapId });
  conn.mapId = null;
  conn.voice = false;
  if (r.members.size === 0) rooms.delete(mapId);
}

function detach(conn) {
  for (const [id] of rooms) leaveRoom(id, conn, false);
  if (conn.userId) {
    const set = byUser.get(conn.userId);
    if (set) {
      set.delete(conn.id);
      if (!set.size) { byUser.delete(conn.userId); broadcastPresence(conn.userId, false); }
    }
  }
}

// 5 Hz room ticker: single consolidated state packet per room.
const ticker = setInterval(() => {
  for (const [mapId, r] of rooms) {
    if (!r.members.size) continue;
    broadcastRoom(mapId, { type: 'room_tick', t: Date.now(), players: roomState(mapId).players });
  }
}, 200);
if (ticker.unref) ticker.unref();

// ---- connection setup ------------------------------------------------------
export function attachRealtime(server) {
  server.on('upgrade', (req, socket) => {
    try {
      const url = new URL(req.url, 'http://localhost');
      if (url.pathname !== '/ws') {
        socket.write('HTTP/1.1 404 Not Found\r\n\r\n');
        return socket.destroy();
      }
      const key = req.headers['sec-websocket-key'];
      if (!key || String(req.headers.upgrade).toLowerCase() !== 'websocket') {
        socket.write('HTTP/1.1 400 Bad Request\r\n\r\n');
        return socket.destroy();
      }
      const accept = crypto.createHash('sha1').update(key + GUID).digest('base64');
      socket.write(
        'HTTP/1.1 101 Switching Protocols\r\n' +
        'Upgrade: websocket\r\n' +
        'Connection: Upgrade\r\n' +
        `Sec-WebSocket-Accept: ${accept}\r\n\r\n`
      );
      socket.setNoDelay(true);
      const conn = { id: newId('ws'), socket, userId: null, mapId: null, closed: false, buffer: Buffer.alloc(0) };
      sockets.set(conn.id, conn);

      socket.on('data', (chunk) => {
        conn.buffer = Buffer.concat([conn.buffer, chunk]);
        if (conn.buffer.length > 1024 * 1024) { socket.destroy(); return; }
        for (;;) {
          const frame = parseFrame(conn.buffer);
          if (!frame) break;
          conn.buffer = conn.buffer.subarray(frame.bytes);
          handleFrame(conn, frame);
        }
      });
      const cleanup = () => {
        if (conn.closed) return;
        conn.closed = true;
        detach(conn);
        sockets.delete(conn.id);
        try { socket.destroy(); } catch { /* ignore */ }
      };
      socket.on('close', cleanup);
      socket.on('error', cleanup);
      socket.on('end', cleanup);
      sendJSON(conn, { type: 'hello', serverTime: nowSec(), version: 1 });
    } catch (e) {
      console.error('[NEXORA] ws upgrade failed', e);
      try { socket.destroy(); } catch { /* ignore */ }
    }
  });
}

function parseFrame(buf) {
  if (buf.length < 2) return null;
  const b0 = buf[0], b1 = buf[1];
  const opcode = b0 & 0x0f;
  const masked = (b1 & 0x80) !== 0;
  let len = b1 & 0x7f;
  let offset = 2;
  if (len === 126) {
    if (buf.length < 4) return null;
    len = buf.readUInt16BE(2); offset = 4;
  } else if (len === 127) {
    if (buf.length < 10) return null;
    len = Number(buf.readBigUInt64BE(2)); offset = 10;
  }
  const maskLen = masked ? 4 : 0;
  if (buf.length < offset + maskLen + len) return null;
  const mask = masked ? buf.subarray(offset, offset + 4) : null;
  const payload = Buffer.from(buf.subarray(offset + maskLen, offset + maskLen + len));
  if (masked) for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i % 4];
  return { opcode, payload, bytes: offset + maskLen + len };
}

function handleFrame(conn, frame) {
  if (frame.opcode === 0x8) { // close
    conn.closed = true;
    detach(conn);
    sockets.delete(conn.id);
    try { conn.socket.end(); } catch { /* ignore */ }
    return;
  }
  if (frame.opcode === 0x9) return sendFrame(conn.socket, 0xA, frame.payload); // ping -> pong
  if (frame.opcode === 0xA) return; // pong
  if (frame.opcode !== 0x1) return;
  let msg;
  try { msg = JSON.parse(frame.payload.toString('utf8')); } catch { return; }
  if (!msg || typeof msg.type !== 'string') return;
  if (msg.type !== 'auth' && !conn.userId) {
    return sendJSON(conn, { type: 'error', error: 'not_authed' });
  }
  const fn = handlers[msg.type];
  if (!fn) return sendJSON(conn, { type: 'error', error: 'unknown_type' });
  try { fn(conn, msg); } catch (e) {
    console.error('[NEXORA] ws handler error', msg.type, e.message);
    sendJSON(conn, { type: 'error', error: 'handler_failed' });
  }
}

export function realtimeStats() {
  return {
    connections: sockets.size,
    onlineUsers: byUser.size,
    rooms: [...rooms.values()].map((r) => ({ mapId: r.mapId, players: r.members.size })),
  };
}

export function closeAll() {
  for (const c of sockets.values()) { try { c.socket.destroy(); } catch { /* ignore */ } }
  sockets.clear(); byUser.clear(); rooms.clear();
}
