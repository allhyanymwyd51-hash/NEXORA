// Users: public shapes, lookups, bans, presence-safe serialisation.
import { get, all, run } from './db.js';
import { notFound } from './util.js';

export const ROLES = ['player', 'creator', 'moderator', 'owner'];
export const ROLE_RANK = { player: 0, creator: 1, moderator: 2, owner: 3 };

export function publicUser(u, extra = {}) {
  if (!u) return null;
  return {
    id: u.id,
    username: u.username,
    avatar: u.avatar || '',
    bio: u.bio || '',
    role: u.role,
    miy: u.miy,
    playSeconds: u.play_seconds,
    createdAt: u.created_at,
    appearance: safeJson(u.appearance, {}),
    ...extra,
  };
}

export function profileUser(u, viewerId) {
  if (!u) return null;
  const base = publicUser(u);
  const followers = get('SELECT COUNT(*) AS c FROM follows WHERE following_id = ?', u.id).c;
  const following = get('SELECT COUNT(*) AS c FROM follows WHERE follower_id = ?', u.id).c;
  const maps = get("SELECT COUNT(*) AS c FROM maps WHERE owner_id = ? AND status = 'published'", u.id).c;
  const likes = get(
    'SELECT COUNT(*) AS c FROM map_likes l JOIN maps m ON m.id = l.map_id WHERE m.owner_id = ?', u.id
  ).c;
  let relationship = 'none';
  if (viewerId && viewerId !== u.id) {
    const f = get(
      `SELECT * FROM friendships WHERE (requester_id = ? AND addressee_id = ?) OR (requester_id = ? AND addressee_id = ?)`,
      viewerId, u.id, u.id, viewerId
    );
    if (f) {
      if (f.status === 'accepted') relationship = 'friends';
      else if (f.status === 'pending') relationship = f.requester_id === viewerId ? 'request_sent' : 'request_received';
      else relationship = 'declined';
    }
    if (get('SELECT 1 AS x FROM blocks WHERE blocker_id = ? AND blocked_id = ?', viewerId, u.id)) relationship = 'blocked';
    if (get('SELECT 1 AS x FROM blocks WHERE blocker_id = ? AND blocked_id = ?', u.id, viewerId)) relationship = 'blocked_by';
  }
  return {
    ...base,
    followers, following, maps, likes, relationship,
    banned: !!u.banned,
  };
}

export function getUserById(id) {
  return get('SELECT * FROM users WHERE id = ?', id);
}

export function mustGetUser(id) {
  const u = getUserById(id);
  if (!u) throw notFound('المستخدم غير موجود');
  return u;
}

export function findUserByLogin(login) {
  return get('SELECT * FROM users WHERE username = ? OR email = ?', login, String(login).toLowerCase());
}

export function searchUsers(q, limit = 20) {
  const like = `%${String(q).replace(/[%_]/g, '')}%`;
  return all(
    'SELECT * FROM users WHERE username LIKE ? AND banned = 0 ORDER BY username LIMIT ?', like, limit
  );
}

export function safeJson(value, fallback) {
  try { return JSON.parse(value); } catch { return fallback; }
}

export function isOwner(u) {
  return !!u && u.role === 'owner';
}

export function rankOf(u) {
  return u ? (ROLE_RANK[u.role] ?? 0) : -1;
}
