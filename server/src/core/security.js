// Password hashing, session tokens, signed links.
import crypto from 'node:crypto';
import { config } from './config.js';

const SCRYPT_N = 16384, SCRYPT_r = 8, SCRYPT_p = 1, KEYLEN = 64;

export function hashPassword(password) {
  const salt = crypto.randomBytes(16);
  const hash = crypto.scryptSync(normalize(password), salt, KEYLEN, { N: SCRYPT_N, r: SCRYPT_r, p: SCRYPT_p });
  return `scrypt$${SCRYPT_N}$${SCRYPT_r}$${SCRYPT_p}$${salt.toString('base64')}$${hash.toString('base64')}`;
}

export function verifyPassword(password, stored) {
  try {
    const parts = String(stored).split('$');
    if (parts.length !== 6 || parts[0] !== 'scrypt') return false;
    const [, N, r, p, saltB64, hashB64] = parts;
    const salt = Buffer.from(saltB64, 'base64');
    const expected = Buffer.from(hashB64, 'base64');
    const actual = crypto.scryptSync(normalize(password), salt, expected.length, {
      N: Number(N), r: Number(r), p: Number(p),
    });
    return crypto.timingSafeEqual(expected, actual);
  } catch {
    return false;
  }
}

function normalize(pw) {
  // Unicode normalization so the same visual password always matches (Arabic input etc.)
  return String(pw).normalize('NFKC');
}

export function sha256(value) {
  return crypto.createHash('sha256').update(value).digest('hex');
}

export function randomToken(bytes = 32) {
  return crypto.randomBytes(bytes).toString('base64url');
}

export function newId(prefix) {
  return `${prefix}_${crypto.randomBytes(9).toString('base64url')}`;
}

export function hmac(value, purpose = '') {
  return crypto.createHmac('sha256', config.secret).update(`${purpose}:${value}`).digest('base64url');
}

// Constant-time-ish compare for HMAC results.
export function safeEqual(a, b) {
  const ba = Buffer.from(String(a));
  const bb = Buffer.from(String(b));
  if (ba.length !== bb.length) return false;
  return crypto.timingSafeEqual(ba, bb);
}

export function signPayload(obj, ttlSeconds = 900) {
  const body = { ...obj, exp: Math.floor(Date.now() / 1000) + ttlSeconds };
  const data = Buffer.from(JSON.stringify(body)).toString('base64url');
  return `${data}.${hmac(data, 'sign')}`;
}

export function verifyPayload(token) {
  if (typeof token !== 'string' || !token.includes('.')) return null;
  const [data, sig] = token.split('.');
  if (!safeEqual(hmac(data, 'sign'), sig)) return null;
  try {
    const obj = JSON.parse(Buffer.from(data, 'base64url').toString('utf8'));
    if (!obj.exp || obj.exp < Math.floor(Date.now() / 1000)) return null;
    return obj;
  } catch {
    return null;
  }
}
