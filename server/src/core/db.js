// NEXORA database layer — SQLite (WAL) via node:sqlite, the same SQL schema works on
// PostgreSQL for horizontal scale-out. All writes go through this module.
import { DatabaseSync } from 'node:sqlite';
import fs from 'node:fs';
import path from 'node:path';
import { config } from './config.js';

let db;

const SCHEMA = `
PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;
PRAGMA synchronous = NORMAL;
PRAGMA busy_timeout = 5000;

CREATE TABLE IF NOT EXISTS users (
  id TEXT PRIMARY KEY,
  username TEXT NOT NULL UNIQUE COLLATE NOCASE,
  email TEXT NOT NULL UNIQUE COLLATE NOCASE,
  password_hash TEXT NOT NULL,
  birth_date TEXT NOT NULL,
  avatar TEXT NOT NULL DEFAULT '',
  bio TEXT NOT NULL DEFAULT '',
  role TEXT NOT NULL DEFAULT 'player',
  banned INTEGER NOT NULL DEFAULT 0,
  ban_reason TEXT NOT NULL DEFAULT '',
  miy INTEGER NOT NULL DEFAULT 0,
  play_seconds INTEGER NOT NULL DEFAULT 0,
  appearance TEXT NOT NULL DEFAULT '{}',
  settings TEXT NOT NULL DEFAULT '{}',
  created_at INTEGER NOT NULL,
  last_login INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_users_role ON users(role);

CREATE TABLE IF NOT EXISTS sessions (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  token_hash TEXT NOT NULL UNIQUE,
  created_at INTEGER NOT NULL,
  expires_at INTEGER NOT NULL,
  device TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);

CREATE TABLE IF NOT EXISTS maps (
  id TEXT PRIMARY KEY,
  owner_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  cover TEXT NOT NULL DEFAULT '',
  game_type TEXT NOT NULL DEFAULT 'open_world',
  privacy TEXT NOT NULL DEFAULT 'public',
  max_players INTEGER NOT NULL DEFAULT 18,
  version INTEGER NOT NULL DEFAULT 1,
  world TEXT NOT NULL DEFAULT '{}',
  status TEXT NOT NULL DEFAULT 'draft',
  visits INTEGER NOT NULL DEFAULT 0,
  is_ai INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,
  published_at INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_maps_status ON maps(status, published_at DESC);
CREATE INDEX IF NOT EXISTS idx_maps_owner ON maps(owner_id);

CREATE TABLE IF NOT EXISTS map_likes (
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  map_id TEXT NOT NULL REFERENCES maps(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL,
  PRIMARY KEY (user_id, map_id)
);
CREATE TABLE IF NOT EXISTS map_favorites (
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  map_id TEXT NOT NULL REFERENCES maps(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL,
  PRIMARY KEY (user_id, map_id)
);
CREATE TABLE IF NOT EXISTS follows (
  follower_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  following_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL,
  PRIMARY KEY (follower_id, following_id)
);
CREATE TABLE IF NOT EXISTS friendships (
  id TEXT PRIMARY KEY,
  requester_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  addressee_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  status TEXT NOT NULL DEFAULT 'pending',
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,
  UNIQUE (requester_id, addressee_id)
);
CREATE INDEX IF NOT EXISTS idx_friend_addr ON friendships(addressee_id, status);
CREATE INDEX IF NOT EXISTS idx_friend_req ON friendships(requester_id, status);

CREATE TABLE IF NOT EXISTS blocks (
  blocker_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  blocked_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL,
  PRIMARY KEY (blocker_id, blocked_id)
);

CREATE TABLE IF NOT EXISTS messages (
  id TEXT PRIMARY KEY,
  channel TEXT NOT NULL,
  channel_id TEXT NOT NULL DEFAULT '',
  sender_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  body TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  deleted INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_msg_channel ON messages(channel, channel_id, created_at DESC);

CREATE TABLE IF NOT EXISTS map_sessions (
  id TEXT PRIMARY KEY,
  map_id TEXT NOT NULL REFERENCES maps(id) ON DELETE CASCADE,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  started_at INTEGER NOT NULL,
  last_credit_at INTEGER NOT NULL,
  credited_minutes INTEGER NOT NULL DEFAULT 0,
  ended_at INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_mapsess_user ON map_sessions(user_id, ended_at);

CREATE TABLE IF NOT EXISTS transactions (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  delta INTEGER NOT NULL,
  balance_after INTEGER NOT NULL,
  reason TEXT NOT NULL,
  ref TEXT NOT NULL UNIQUE,
  meta TEXT NOT NULL DEFAULT '{}',
  created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_tx_user ON transactions(user_id, created_at DESC);

CREATE TABLE IF NOT EXISTS shop_items (
  id TEXT PRIMARY KEY,
  name_ar TEXT NOT NULL,
  name_en TEXT NOT NULL,
  category TEXT NOT NULL,
  price INTEGER NOT NULL,
  color TEXT NOT NULL DEFAULT '#8899ff',
  rarity TEXT NOT NULL DEFAULT 'common',
  body_part TEXT NOT NULL DEFAULT 'torso',
  mesh TEXT NOT NULL DEFAULT '',
  active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS inventory (
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  item_id TEXT NOT NULL REFERENCES shop_items(id) ON DELETE CASCADE,
  acquired_at INTEGER NOT NULL,
  PRIMARY KEY (user_id, item_id)
);

CREATE TABLE IF NOT EXISTS game_passes (
  id TEXT PRIMARY KEY,
  map_id TEXT NOT NULL REFERENCES maps(id) ON DELETE CASCADE,
  creator_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  price INTEGER NOT NULL,
  image TEXT NOT NULL DEFAULT '',
  perks TEXT NOT NULL DEFAULT '[]',
  published INTEGER NOT NULL DEFAULT 1,
  created_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS pass_ownership (
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  pass_id TEXT NOT NULL REFERENCES game_passes(id) ON DELETE CASCADE,
  purchased_at INTEGER NOT NULL,
  price_paid INTEGER NOT NULL,
  PRIMARY KEY (user_id, pass_id)
);

CREATE TABLE IF NOT EXISTS notifications (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  type TEXT NOT NULL,
  title TEXT NOT NULL,
  body TEXT NOT NULL DEFAULT '',
  data TEXT NOT NULL DEFAULT '{}',
  read_at INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_notif_user ON notifications(user_id, created_at DESC);

CREATE TABLE IF NOT EXISTS reports (
  id TEXT PRIMARY KEY,
  reporter_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  target_type TEXT NOT NULL,
  target_id TEXT NOT NULL,
  reason TEXT NOT NULL,
  details TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'open',
  created_at INTEGER NOT NULL,
  reviewed_by TEXT,
  reviewed_at INTEGER NOT NULL DEFAULT 0,
  action TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_reports_status ON reports(status, created_at DESC);

CREATE TABLE IF NOT EXISTS audit_log (
  id TEXT PRIMARY KEY,
  actor_id TEXT NOT NULL DEFAULT '',
  action TEXT NOT NULL,
  target TEXT NOT NULL DEFAULT '',
  detail TEXT NOT NULL DEFAULT '{}',
  ip TEXT NOT NULL DEFAULT '',
  created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_log(created_at DESC);

CREATE TABLE IF NOT EXISTS ai_jobs (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  map_id TEXT,
  kind TEXT NOT NULL DEFAULT 'create',
  prompt TEXT NOT NULL,
  game_type TEXT NOT NULL DEFAULT 'open_world',
  world_size TEXT NOT NULL DEFAULT 'medium',
  detail TEXT NOT NULL DEFAULT 'detailed',
  quality TEXT NOT NULL DEFAULT 'high',
  max_players INTEGER NOT NULL DEFAULT 18,
  status TEXT NOT NULL DEFAULT 'queued',
  progress INTEGER NOT NULL DEFAULT 0,
  stage TEXT NOT NULL DEFAULT '',
  plan TEXT NOT NULL DEFAULT '{}',
  summary TEXT NOT NULL DEFAULT '',
  error TEXT NOT NULL DEFAULT '',
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_jobs_user ON ai_jobs(user_id, created_at DESC);

CREATE TABLE IF NOT EXISTS server_settings (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS email_tokens (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  kind TEXT NOT NULL,
  token_hash TEXT NOT NULL UNIQUE,
  expires_at INTEGER NOT NULL,
  used_at INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL
);
`;

export function openDb() {
  if (db) return db;
  const dir = path.dirname(config.dbPath);
  fs.mkdirSync(dir, { recursive: true });
  db = new DatabaseSync(config.dbPath);
  db.exec(SCHEMA);
  seedShop(db);
  return db;
}

// Initial store catalogue: intentionally small, curated and non-repetitive (§11).
const SHOP_SEED = [
  ['shirt_noir', 'قميص نوار', 'Noir Shirt', 'shirt', 120, '#1b1e2b', 'common', 'torso'],
  ['shirt_cyan', 'قميص سايان', 'Cyan Weave Shirt', 'shirt', 150, '#1fb6c9', 'rare', 'torso'],
  ['shirt_violet', 'قميص فيوليت', 'Violet Thread Shirt', 'shirt', 180, '#7c4dff', 'rare', 'torso'],
  ['pants_slate', 'بنطال سليت', 'Slate Pants', 'pants', 110, '#2b2f3d', 'common', 'legs'],
  ['pants_cargo', 'بنطال كارجو', 'Cargo Pants', 'pants', 160, '#3a3f2e', 'common', 'legs'],
  ['shoes_runner', 'حذاء رَنر', 'Runner Shoes', 'shoes', 90, '#e6e9f2', 'common', 'feet'],
  ['shoes_obsidian', 'حذاء أوبسيديان', 'Obsidian Boots', 'shoes', 210, '#101218', 'rare', 'feet'],
  ['cap_nex', 'قبعة NEX', 'NEX Cap', 'hat', 130, '#2c3e66', 'common', 'head'],
  ['visor_orbit', 'نظارة أوربِت', 'Orbit Visor', 'glasses', 240, '#38e1ff', 'epic', 'face'],
  ['glass_halo', 'نظارة هالو', 'Halo Glasses', 'glasses', 200, '#9b6bff', 'rare', 'face'],
  ['acc_watch', 'ساعة بلس', 'Pulse Watch', 'accessory', 260, '#5b6bff', 'epic', 'arm'],
  ['acc_chain', 'سلسلة إيثر', 'Ether Chain', 'accessory', 300, '#d8b45a', 'legendary', 'neck'],
  ['hair_short', 'تسريحة كلاسيك', 'Classic Cut', 'hair', 80, '#241a16', 'common', 'hair'],
  ['hair_luxe', 'تسريحة لوكس', 'Luxe Flow', 'hair', 220, '#3a2a20', 'rare', 'hair'],
  ['body_slim', 'هيكل رشيق', 'Slim Frame', 'body', 400, '#6c7bd9', 'epic', 'body'],
  ['body_power', 'هيكل قوي', 'Power Frame', 'body', 400, '#a05b4b', 'epic', 'body'],
  ['emote_wave', 'رقصة الموجة', 'Wave Emote', 'emote', 100, '#7bd9c0', 'common', 'emote'],
  ['aura_nex', 'هالة NEX', 'NEX Aura', 'aura', 650, '#7c4dff', 'legendary', 'aura'],
];

function seedShop(database) {
  const stmt = database.prepare(
    'INSERT OR IGNORE INTO shop_items(id,name_ar,name_en,category,price,color,rarity,body_part) VALUES (?,?,?,?,?,?,?,?)'
  );
  for (const row of SHOP_SEED) stmt.run(...row);
}

export function getDb() {
  return openDb();
}

export function all(sql, ...params) {
  return getDb().prepare(sql).all(...params);
}
export function get(sql, ...params) {
  return getDb().prepare(sql).get(...params);
}
export function run(sql, ...params) {
  return getDb().prepare(sql).run(...params);
}

// Synchronous transaction with rollback on error.
export function tx(fn) {
  const d = getDb();
  d.exec('BEGIN IMMEDIATE');
  try {
    const out = fn(d);
    d.exec('COMMIT');
    return out;
  } catch (e) {
    try { d.exec('ROLLBACK'); } catch { /* ignore */ }
    throw e;
  }
}

export function getSetting(key, fallback = null) {
  const row = get('SELECT value FROM server_settings WHERE key = ?', key);
  if (!row) return fallback;
  try { return JSON.parse(row.value); } catch { return fallback; }
}

export function setSetting(key, value) {
  run(
    'INSERT INTO server_settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value',
    key, JSON.stringify(value)
  );
}

export function closeDb() {
  if (db) { try { db.close(); } catch { /* ignore */ } db = undefined; }
}
