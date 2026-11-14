// Minimal, explicit router: method + path pattern => handler. No magic, no deps.
import { HttpError, json, readBody, rateLimit, clientIp } from '../core/util.js';
import { userFromToken, bearer, guard } from '../core/auth.js';

const routes = [];

export function route(method, pattern, handler, opts = {}) {
  const keys = [];
  const regex = new RegExp(
    '^' + pattern.replace(/\/:([a-zA-Z_]+)/g, (_, k) => { keys.push(k); return '/([^/]+)'; }) + '/?$'
  );
  routes.push({ method, regex, keys, handler, opts });
}

export const get = (p, h, o) => route('GET', p, h, o);
export const post = (p, h, o) => route('POST', p, h, o);
export const patch = (p, h, o) => route('PATCH', p, h, o);
export const del = (p, h, o) => route('DELETE', p, h, o);
export const put = (p, h, o) => route('PUT', p, h, o);

export function match(method, pathname) {
  let pathMatched = false;
  for (const r of routes) {
    const m = r.regex.exec(pathname);
    if (!m) continue;
    pathMatched = true;
    if (r.method !== method) continue;
    const params = {};
    r.keys.forEach((k, i) => { params[k] = decodeURIComponent(m[i + 1]); });
    return { route: r, params };
  }
  return pathMatched ? { methodNotAllowed: true } : null;
}

export async function handle(req, res) {
  const url = new URL(req.url, 'http://localhost');
  const pathname = url.pathname.replace(/\/+$/, '') || '/';
  const ip = clientIp(req);

  // CORS for local tooling / web dashboards. Never sends credentials without a token.
  res.setHeader('access-control-allow-origin', '*');
  res.setHeader('access-control-allow-headers', 'content-type, authorization');
  res.setHeader('access-control-allow-methods', 'GET,POST,PATCH,DELETE,PUT,OPTIONS');
  if (req.method === 'OPTIONS') { res.writeHead(204); res.end(); return; }

  try {
    const found = match(req.method, pathname);
    if (!found) throw new HttpError(404, 'not_found', 'المسار غير موجود');
    if (found.methodNotAllowed) throw new HttpError(405, 'method_not_allowed', 'طريقة غير مدعومة');
    const { route: r, params } = found;

    if (r.opts.rate) rateLimit(`${r.opts.rate}:${ip}:${pathname}`, r.opts.rateMax || 30, r.opts.rateMs || 60000);

    const token = bearer(req);
    let user = userFromToken(token);
    if (user && user.banned) {
      // Banned users keep a valid token only to read the ban reason.
      if (!(pathname === '/api/auth/me')) throw new HttpError(403, 'banned', user.ban_reason || 'الحساب محظور');
    }
    const body = ['POST', 'PATCH', 'PUT', 'DELETE'].includes(req.method) ? await readBody(req) : {};
    const ctx = {
      req, res, url, params, body, ip, token, user, query: url.searchParams,
      out: (status, payload) => json(res, status, payload),
    };
    guard(ctx, r.opts);
    await r.handler(ctx);
    if (!res.writableEnded) json(res, 200, { ok: true });
  } catch (err) {
    if (err instanceof HttpError) {
      return json(res, err.status, { ok: false, error: err.code, message: err.message, ...(err.extra || {}) });
    }
    console.error('[NEXORA] unhandled', err);
    return json(res, 500, { ok: false, error: 'internal', message: 'خطأ في السيرفر' });
  }
}
