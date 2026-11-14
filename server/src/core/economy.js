// Miy economy: server-authoritative playtime crediting. The client can never add balance.
import { get, run, all, tx } from './db.js';
import { config } from './config.js';
import { newId } from './security.js';
import { nowSec, badRequest } from './util.js';
import { notify } from './auth.js';

export function balanceOf(userId) {
  return get('SELECT miy FROM users WHERE id = ?', userId)?.miy ?? 0;
}

// Atomic balance mutation with a ledger row. `ref` is a unique idempotency key, so a retried
// request can never pay twice.
export function credit(userId, delta, reason, ref, meta = {}) {
  return tx(() => {
    const existing = get('SELECT * FROM transactions WHERE ref = ?', ref);
    if (existing) return { duplicate: true, balance: existing.balance_after, delta: existing.delta };
    const u = get('SELECT * FROM users WHERE id = ?', userId);
    if (!u) throw badRequest('no_user', 'المستخدم غير موجود');
    const next = u.miy + delta;
    if (next < 0) throw badRequest('insufficient_funds', 'رصيد Miy غير كافٍ');
    run('UPDATE users SET miy = ? WHERE id = ?', next, userId);
    run(
      'INSERT INTO transactions(id,user_id,delta,balance_after,reason,ref,meta,created_at) VALUES(?,?,?,?,?,?,?,?)',
      newId('tx'), userId, delta, next, reason, ref, JSON.stringify(meta), nowSec()
    );
    return { duplicate: false, balance: next, delta };
  });
}

export function minutesToday(userId) {
  const start = new Date(); start.setUTCHours(0, 0, 0, 0);
  const row = get(
    'SELECT COALESCE(SUM(credited_minutes),0) AS m FROM map_sessions WHERE user_id = ? AND started_at >= ?',
    userId, Math.floor(start.getTime() / 1000)
  );
  return row?.m ?? 0;
}

export function startPlaySession(mapId, userId) {
  const now = nowSec();
  const open = get('SELECT * FROM map_sessions WHERE user_id = ? AND ended_at = 0', userId);
  if (open) {
    if (open.map_id === mapId) return { sessionId: open.id, resumed: true };
    endPlaySession(open.id, userId);
  }
  const id = newId('ms');
  run(
    'INSERT INTO map_sessions(id,map_id,user_id,started_at,last_credit_at,credited_minutes) VALUES(?,?,?,?,?,0)',
    id, mapId, userId, now, now
  );
  return { sessionId: id, resumed: false };
}

// Heartbeat: the client calls this every ~20s while actually inside a map. The server owns
// the clock: it credits only whole elapsed minutes of an open server-side session, capped per day.
export function heartbeat(sessionId, userId) {
  const s = get('SELECT * FROM map_sessions WHERE id = ? AND user_id = ? AND ended_at = 0', sessionId, userId);
  if (!s) throw badRequest('no_session', 'جلسة اللعب غير موجودة');
  const now = nowSec();
  const dueMinutes = Math.floor((now - s.last_credit_at) / 60);
  if (dueMinutes <= 0) {
    return { credited: 0, minutes: s.credited_minutes, balance: balanceOf(userId), creditsToday: minutesToday(userId) };
  }
  const cap = config.economy.maxMinutesPerDay;
  const used = minutesToday(userId);
  const allow = Math.max(0, Math.min(dueMinutes, cap - used));
  run(
    'UPDATE map_sessions SET last_credit_at = last_credit_at + ?, credited_minutes = credited_minutes + ? WHERE id = ?',
    allow * 60, allow, s.id
  );
  let balance = balanceOf(userId);
  if (allow > 0) {
    const amount = allow * config.economy.miyPerMinute;
    const res = credit(userId, amount, 'playtime', `play:${s.id}:${s.credited_minutes + allow}`, {
      mapId: s.map_id, minutes: allow,
    });
    balance = res.balance;
    notify(userId, 'economy', `+${amount} Miy`, `مكافأة اللعب داخل الماب (${allow} دقيقة)`);
  }
  return {
    credited: allow * config.economy.miyPerMinute,
    minutes: s.credited_minutes + allow,
    balance,
    capped: allow < dueMinutes,
    creditsToday: used + allow,
    dailyCapMinutes: cap,
  };
}

export function endPlaySession(sessionId, userId) {
  const s = get('SELECT * FROM map_sessions WHERE id = ? AND user_id = ? AND ended_at = 0', sessionId, userId);
  if (!s) return { ok: true, alreadyEnded: true, credited: 0 };
  let last = { credited: 0, balance: balanceOf(userId) };
  try { last = heartbeat(sessionId, userId); } catch { /* ignore */ }
  const now = nowSec();
  run('UPDATE map_sessions SET ended_at = ? WHERE id = ?', now, s.id);
  run('UPDATE users SET play_seconds = play_seconds + ? WHERE id = ?', Math.max(0, now - s.started_at), userId);
  return { ok: true, secondsPlayed: now - s.started_at, ...last };
}

export function ledgerFor(userId, limit = 60) {
  return all('SELECT * FROM transactions WHERE user_id = ? ORDER BY created_at DESC LIMIT ?', userId, limit)
    .map((t) => ({
      id: t.id, delta: t.delta, balance: t.balance_after, reason: t.reason,
      meta: JSON.parse(t.meta || '{}'), at: t.created_at,
    }));
}
