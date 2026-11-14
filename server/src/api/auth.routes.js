// Auth + account routes.
import { get, run } from '../core/db.js';
import {
  badRequest, conflict, unauthorized, forbidden, nowSec, validateUsername, validateEmail,
  validatePassword, validateBirthDate, cleanText, rateLimit, HttpError,
} from '../core/util.js';
import { hashPassword, verifyPassword, randomToken, sha256, newId } from '../core/security.js';
import { createSession, destroySession, bearer, audit, notify } from '../core/auth.js';
import { publicUser, getUserById, profileUser, safeJson } from '../core/users.js';
import { post, get as getRoute, patch } from './router.js';
import { config } from '../core/config.js';

const DEFAULT_SETTINGS = {
  language: 'ar',
  quality: 'auto',
  sensitivity: 1.0,
  sound: 0.8,
  music: 0.5,
  micEnabled: false,
  spatialVoice: true,
  chatPrivacy: 'friends',
  friendRequests: 'everyone',
  notifications: { friends: true, messages: true, invites: true, economy: true, system: true },
};

const DEFAULT_APPEARANCE = {
  skin: '#c98e6a', hair: '#241a16', shirt: '#5b6bff', pants: '#2b2f3d', shoes: '#e6e9f2',
  hat: '', glasses: '', accessory: '', bodyType: 'slim', ownedLook: {},
};

function settingsOf(u) {
  return { ...DEFAULT_SETTINGS, ...safeJson(u.settings, {}) };
}

function mePayload(u) {
  const unread = get('SELECT COUNT(*) AS c FROM notifications WHERE user_id = ? AND read_at = 0', u.id).c;
  const friendReqs = get(
    "SELECT COUNT(*) AS c FROM friendships WHERE addressee_id = ? AND status = 'pending'", u.id
  ).c;
  return {
    ...publicUser(u),
    settings: settingsOf(u),
    unreadNotifications: unread,
    friendRequests: friendReqs,
    isOwner: u.role === 'owner',
  };
}

post('/api/auth/register', async (ctx) => {
  rateLimit(`register:${ctx.ip}`, 8, 60 * 60 * 1000);
  const { body } = ctx;
  const username = validateUsername(body.username);
  const email = validateEmail(body.email);
  const password = validatePassword(body.password);
  if (String(body.confirmPassword ?? body.password) !== password) {
    throw badRequest('password_mismatch', 'تأكيد كلمة المرور غير مطابق');
  }
  const birthDate = validateBirthDate(body.birthDate);
  if (body.acceptedTerms !== true) throw badRequest('terms_required', 'يجب الموافقة على الشروط وسياسة الخصوصية');
  const avatar = String(body.avatar || '').slice(0, 400000);
  if (avatar && !/^data:image\/(png|jpe?g|webp);base64,/.test(avatar)) {
    throw badRequest('bad_avatar', 'صيغة الصورة غير مدعومة');
  }
  if (avatar.length > 600000) throw badRequest('bad_avatar', 'حجم الصورة كبير جداً');
  const appearance = body.appearance && typeof body.appearance === 'object'
    ? JSON.stringify({ ...DEFAULT_APPEARANCE, ...body.appearance }).slice(0, 4000) : JSON.stringify(DEFAULT_APPEARANCE);
  if (get('SELECT id FROM users WHERE username = ?', username)) throw conflict('username_taken', 'اسم المستخدم محجوز');
  if (get('SELECT id FROM users WHERE email = ?', email)) throw conflict('email_taken', 'البريد الإلكتروني مستخدم بالفعل');

  const id = newId('usr');
  const now = nowSec();
  run(
    `INSERT INTO users(id,username,email,password_hash,birth_date,avatar,role,appearance,settings,created_at)
     VALUES(?,?,?,?,?,?,?,?,?,?)`,
    id, username, email, hashPassword(password), birthDate, avatar, 'player', appearance,
    JSON.stringify(DEFAULT_SETTINGS), now
  );
  const user = getUserById(id);
  const session = createSession(id, ctx.req.headers['user-agent'] || '');
  audit(id, 'account.register', id, { username }, ctx.ip);
  notify(id, 'system', 'مرحباً في NEXORA', 'حسابك جاهز. ابدأ بصناعة ماب أو استكشاف المابات المنشورة.');
  ctx.out(201, { ok: true, token: session.token, user: mePayload(user) });
}, { rate: 'register', rateMax: 8, rateMs: 3600000 });

post('/api/auth/login', async (ctx) => {
  const login = String(ctx.body.login || ctx.body.username || ctx.body.email || '').trim();
  const password = String(ctx.body.password || '');
  const remember = ctx.body.remember !== false;
  if (!login || !password) throw badRequest('missing_credentials', 'أدخل بيانات الدخول');
  const user = get('SELECT * FROM users WHERE username = ? OR email = ?', login, login.toLowerCase());
  rateLimit(`login:${ctx.ip}:${login.toLowerCase()}`, 10, 10 * 60 * 1000);
  // Always run a hash comparison to avoid user-enumeration timing leaks.
  const stored = user ? user.password_hash : hashPassword('placeholder-password1');
  const ok = verifyPassword(password, stored) && !!user;
  if (!ok) {
    audit(user?.id || '', 'account.login_failed', login.slice(0, 40), {}, ctx.ip);
    throw unauthorized('اسم المستخدم أو كلمة المرور غير صحيحة');
  }
  if (user.banned) throw forbidden(user.ban_reason || 'الحساب محظور — تواصل مع الدعم');
  run('UPDATE users SET last_login = ? WHERE id = ?', nowSec(), user.id);
  if (!remember) run('DELETE FROM sessions WHERE user_id = ?', user.id);
  const session = createSession(user.id, ctx.req.headers['user-agent'] || '');
  audit(user.id, 'account.login', user.id, {}, ctx.ip);
  ctx.out(200, { ok: true, token: session.token, user: mePayload(getUserById(user.id)) });
}, { rate: 'login', rateMax: 20, rateMs: 600000 });

post('/api/auth/logout', async (ctx) => {
  destroySession(ctx.token);
  ctx.out(200, { ok: true });
}, { auth: true });

getRoute('/api/auth/me', async (ctx) => {
  if (!ctx.user) throw unauthorized();
  if (ctx.user.banned) {
    return ctx.out(403, {
      ok: false, error: 'banned', message: ctx.user.ban_reason || 'الحساب محظور',
      banned: true,
    });
  }
  const u = getUserById(ctx.user.id);
  ctx.out(200, { ok: true, user: mePayload(u) });
});

patch('/api/auth/password', async (ctx) => {
  const u = getUserById(ctx.user.id);
  const current = String(ctx.body.currentPassword || '');
  if (!verifyPassword(current, u.password_hash)) throw badRequest('wrong_password', 'كلمة المرور الحالية غير صحيحة');
  const next = validatePassword(ctx.body.newPassword);
  if (String(ctx.body.confirmPassword ?? next) !== next) throw badRequest('password_mismatch', 'تأكيد كلمة المرور غير مطابق');
  run('UPDATE users SET password_hash = ? WHERE id = ?', hashPassword(next), u.id);
  run('DELETE FROM sessions WHERE user_id = ? AND token_hash != ?', u.id, sha256(ctx.token));
  audit(u.id, 'account.password_change', u.id, {}, ctx.ip);
  notify(u.id, 'system', 'تم تغيير كلمة المرور', 'تم تسجيل الخروج من الأجهزة الأخرى.');
  ctx.out(200, { ok: true });
}, { auth: true });

// Password reset: request a one-time code. Delivery (email) is configured server-side;
// until then the request is recorded and returns a generic response (no account probing).
post('/api/auth/forgot', async (ctx) => {
  rateLimit(`forgot:${ctx.ip}`, 6, 60 * 60 * 1000);
  const email = String(ctx.body.email || '').trim().toLowerCase();
  const user = get('SELECT * FROM users WHERE email = ?', email);
  if (user) {
    const token = randomToken(24);
    const id = newId('eml');
    run(
      'INSERT INTO email_tokens(id,user_id,kind,token_hash,expires_at,created_at) VALUES(?,?,?,?,?,?)',
      id, user.id, 'password_reset', sha256(token), nowSec() + 3600, nowSec()
    );
    audit(user.id, 'account.reset_requested', user.id, {}, ctx.ip);
    console.log(`[NEXORA] password reset requested for ${email} (token issued, exp 1h)`);
  }
  ctx.out(200, {
    ok: true,
    message: 'إذا كان البريد مسجلاً ستصلك رسالة لإعادة التعيين.',
    delivery: config.ai.baseUrl ? 'queued' : 'not_configured',
  });
});

post('/api/auth/reset', async (ctx) => {
  rateLimit(`reset:${ctx.ip}`, 10, 60 * 60 * 1000);
  const token = String(ctx.body.token || '');
  const next = validatePassword(ctx.body.newPassword);
  const row = get(
    'SELECT * FROM email_tokens WHERE token_hash = ? AND kind = ? AND used_at = 0 AND expires_at > ?',
    sha256(token), 'password_reset', nowSec()
  );
  if (!row) throw badRequest('bad_token', 'رابط إعادة التعيين غير صالح أو منتهي');
  run('UPDATE users SET password_hash = ? WHERE id = ?', hashPassword(next), row.user_id);
  run('UPDATE email_tokens SET used_at = ? WHERE id = ?', nowSec(), row.id);
  run('DELETE FROM sessions WHERE user_id = ?', row.user_id);
  audit(row.user_id, 'account.password_reset', row.user_id, {}, ctx.ip);
  notify(row.user_id, 'system', 'تمت إعادة تعيين كلمة المرور', 'سجّل الدخول بكلمة المرور الجديدة.');
  ctx.out(200, { ok: true });
});

// Profile / settings -------------------------------------------------------
patch('/api/me/profile', async (ctx) => {
  const u = getUserById(ctx.user.id);
  const fields = [];
  const values = [];
  if (ctx.body.username !== undefined && ctx.body.username !== u.username) {
    const username = validateUsername(ctx.body.username);
    if (get('SELECT id FROM users WHERE username = ? AND id != ?', username, u.id)) {
      throw conflict('username_taken', 'اسم المستخدم محجوز');
    }
    fields.push('username = ?'); values.push(username);
  }
  if (ctx.body.bio !== undefined) { fields.push('bio = ?'); values.push(cleanText(ctx.body.bio, 200, 'النبذة')); }
  if (ctx.body.avatar !== undefined) {
    const avatar = String(ctx.body.avatar);
    if (avatar && !/^data:image\/(png|jpe?g|webp);base64,/.test(avatar)) throw badRequest('bad_avatar', 'صيغة الصورة غير مدعومة');
    if (avatar.length > 600000) throw badRequest('bad_avatar', 'حجم الصورة كبير جداً');
    fields.push('avatar = ?'); values.push(avatar);
  }
  if (ctx.body.appearance !== undefined) {
    const merged = { ...DEFAULT_APPEARANCE, ...safeJson(u.appearance, {}), ...ctx.body.appearance };
    fields.push('appearance = ?'); values.push(JSON.stringify(merged).slice(0, 4000));
  }
  if (!fields.length) return ctx.out(200, { ok: true, user: mePayload(u) });
  values.push(u.id);
  run(`UPDATE users SET ${fields.join(', ')} WHERE id = ?`, ...values);
  ctx.out(200, { ok: true, user: mePayload(getUserById(u.id)) });
}, { auth: true });

patch('/api/me/settings', async (ctx) => {
  const u = getUserById(ctx.user.id);
  const current = settingsOf(u);
  const incoming = ctx.body && typeof ctx.body === 'object' ? ctx.body : {};
  const merged = { ...current, ...incoming };
  if (merged.language !== 'ar' && merged.language !== 'en') merged.language = 'ar';
  if (!['low', 'medium', 'high', 'ultra', 'auto'].includes(merged.quality)) merged.quality = 'auto';
  merged.sensitivity = Math.min(3, Math.max(0.2, Number(merged.sensitivity) || 1));
  merged.sound = Math.min(1, Math.max(0, Number(merged.sound) ?? 0.8));
  merged.music = Math.min(1, Math.max(0, Number(merged.music) ?? 0.5));
  run('UPDATE users SET settings = ? WHERE id = ?', JSON.stringify(merged).slice(0, 4000), u.id);
  ctx.out(200, { ok: true, settings: merged });
}, { auth: true });

getRoute('/api/me/notifications', async (ctx) => {
  const { notificationsFor } = await import('../core/auth.js');
  ctx.out(200, { ok: true, notifications: notificationsFor(ctx.user.id, 60) });
}, { auth: true });

post('/api/me/notifications/read', async (ctx) => {
  const { markNotificationsRead } = await import('../core/auth.js');
  markNotificationsRead(ctx.user.id, ctx.body.ids);
  ctx.out(200, { ok: true });
}, { auth: true });

// Real email delivery is not configured in this environment; report the truth.
getRoute('/api/meta', async (ctx) => {
  ctx.out(200, {
    ok: true,
    name: 'NEXORA',
    version: config.version,
    time: nowSec(),
    emailDelivery: false,
    aiProvider: config.ai.baseUrl ? 'remote' : 'local-planner',
    economy: config.economy,
  });
});
