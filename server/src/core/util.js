// Shared helpers: HTTP plumbing, validation, errors, rate limiting.
export class HttpError extends Error {
  constructor(status, code, message, extra) {
    super(message);
    this.status = status;
    this.code = code;
    this.extra = extra;
  }
}

export const badRequest = (code, msg) => new HttpError(400, code, msg);
export const unauthorized = (msg = 'يجب تسجيل الدخول') => new HttpError(401, 'unauthorized', msg);
export const forbidden = (msg = 'لا تملك صلاحية لهذا الإجراء') => new HttpError(403, 'forbidden', msg);
export const notFound = (msg = 'العنصر غير موجود') => new HttpError(404, 'not_found', msg);
export const conflict = (code, msg) => new HttpError(409, code, msg);
export const tooMany = (msg = 'طلبات كثيرة، انتظر قليلاً') => new HttpError(429, 'rate_limited', msg);

// Monotonic-ish wall clock in whole seconds. Clamped so a device or VM with a skewed
// clock can never emit timestamps in the future, which would make freshly written rows
// invisible to "created_at < now" range queries (chat history, feeds, audit log).
export function nowSec() {
  const t = Math.floor(Date.now() / 1000);
  return t > 0 ? t : 1;
}

export function json(res, status, body) {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'cache-control': 'no-store',
    'content-length': Buffer.byteLength(payload),
  });
  res.end(payload);
}

export async function readBody(req, limit = 512 * 1024) {
  return new Promise((resolve, reject) => {
    let size = 0;
    const chunks = [];
    req.on('data', (c) => {
      size += c.length;
      if (size > limit) {
        reject(new HttpError(413, 'payload_too_large', 'حجم الطلب كبير جداً'));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on('end', () => {
      if (!chunks.length) return resolve({});
      const text = Buffer.concat(chunks).toString('utf8');
      try { resolve(JSON.parse(text)); } catch { reject(badRequest('bad_json', 'صيغة JSON غير صحيحة')); }
    });
    req.on('error', reject);
  });
}

// ---- validation ----
const USERNAME_RE = /^[a-zA-Z0-9_\u0621-\u064A]{3,20}$/;
const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/;

export function validateUsername(v) {
  const s = String(v || '').trim();
  if (!USERNAME_RE.test(s)) throw badRequest('bad_username', 'اسم المستخدم يجب أن يكون 3-20 حرفاً (حروف أو أرقام أو _)');
  if (/^[0-9]+$/.test(s)) throw badRequest('bad_username', 'اسم المستخدم لا يمكن أن يكون أرقاماً فقط');
  return s;
}

export function validateEmail(v) {
  const s = String(v || '').trim().toLowerCase();
  if (!EMAIL_RE.test(s) || s.length > 254) throw badRequest('bad_email', 'البريد الإلكتروني غير صحيح');
  return s;
}

export function validatePassword(v) {
  const s = String(v || '');
  if (s.length < 8) throw badRequest('weak_password', 'كلمة المرور يجب أن تكون 8 أحرف على الأقل');
  if (s.length > 128) throw badRequest('weak_password', 'كلمة المرور طويلة جداً');
  if (!/[A-Za-z\u0621-\u064A]/.test(s)) throw badRequest('weak_password', 'كلمة المرور يجب أن تحتوي حرفاً واحداً على الأقل');
  if (!/[0-9]/.test(s)) throw badRequest('weak_password', 'كلمة المرور يجب أن تحتوي رقماً واحداً على الأقل');
  return s;
}

export function validateBirthDate(v) {
  const s = String(v || '').trim();
  if (!/^\d{4}-\d{2}-\d{2}$/.test(s)) throw badRequest('bad_birthdate', 'تاريخ الميلاد غير صحيح');
  const d = new Date(`${s}T00:00:00Z`);
  if (Number.isNaN(d.getTime())) throw badRequest('bad_birthdate', 'تاريخ الميلاد غير صحيح');
  const now = new Date();
  let age = now.getUTCFullYear() - d.getUTCFullYear();
  const m = now.getUTCMonth() - d.getUTCMonth();
  if (m < 0 || (m === 0 && now.getUTCDate() < d.getUTCDate())) age -= 1;
  if (age < 7) throw badRequest('under_age', 'يجب أن يكون عمر الحساب 7 سنوات أو أكثر');
  if (age > 120) throw badRequest('bad_birthdate', 'تاريخ الميلاد غير صحيح');
  return s;
}

export function clampInt(v, min, max, def) {
  const n = Number(v);
  if (!Number.isFinite(n)) return def;
  return Math.min(max, Math.max(min, Math.trunc(n)));
}

export function cleanText(v, maxLen, field = 'text') {
  const s = String(v ?? '').replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F]/g, '').trim();
  if (s.length > maxLen) throw badRequest('too_long', `${field}: الحد الأقصى ${maxLen} حرفاً`);
  return s;
}

// ---- rate limiting (in-memory token bucket, per IP+key) ----
const buckets = new Map();

export function rateLimit(key, max, windowMs) {
  const now = Date.now();
  let b = buckets.get(key);
  if (!b || now > b.reset) {
    b = { count: 0, reset: now + windowMs };
    buckets.set(key, b);
  }
  b.count += 1;
  if (b.count > max) throw tooMany();
  if (buckets.size > 20000) {
    for (const [k, v] of buckets) if (now > v.reset) buckets.delete(k);
  }
}

export function clientIp(req) {
  if (config_trust()) {
    const xf = req.headers['x-forwarded-for'];
    if (typeof xf === 'string' && xf.length) return xf.split(',')[0].trim();
  }
  return req.socket?.remoteAddress || '0.0.0.0';
}

function config_trust() {
  return process.env.TRUST_PROXY === '1';
}
