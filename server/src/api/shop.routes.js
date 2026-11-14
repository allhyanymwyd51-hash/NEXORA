// Shops, inventory, appearances, economy endpoints.
import { get, run, all } from '../core/db.js';
import { badRequest, conflict, notFound, nowSec, clampInt } from '../core/util.js';
import { notify } from '../core/auth.js';
import { safeJson } from '../core/users.js';
import { credit, ledgerFor } from '../core/economy.js';
import { get as getRoute, post } from './router.js';
import { newId } from '../core/security.js';

getRoute('/api/shop', async (ctx) => {
  const rows = all('SELECT * FROM shop_items WHERE active = 1 ORDER BY category, price');
  const owned = new Set(ctx.user ? all('SELECT item_id FROM inventory WHERE user_id = ?', ctx.user.id).map((r) => r.item_id) : []);
  const items = rows.map((r) => ({
    id: r.id, nameAr: r.name_ar, nameEn: r.name_en, category: r.category, price: r.price,
    color: r.color, rarity: r.rarity, bodyPart: r.body_part, owned: owned.has(r.id),
  }));
  const categories = [...new Set(items.map((i) => i.category))];
  ctx.out(200, { ok: true, items, categories, currency: 'Miy', balance: ctx.user?.miy ?? 0 });
});

post('/api/shop/buy', async (ctx) => {
  const item = get('SELECT * FROM shop_items WHERE id = ? AND active = 1', String(ctx.body.itemId || ''));
  if (!item) throw notFound('العنصر غير موجود');
  if (get('SELECT 1 AS x FROM inventory WHERE user_id = ? AND item_id = ?', ctx.user.id, item.id)) {
    throw conflict('already_owned', 'تملك هذا العنصر بالفعل');
  }
  const res = credit(ctx.user.id, -item.price, 'shop', `shop:${item.id}:${ctx.user.id}`, { itemId: item.id });
  run('INSERT OR IGNORE INTO inventory(user_id,item_id,acquired_at) VALUES(?,?,?)', ctx.user.id, item.id, nowSec());
  notify(ctx.user.id, 'economy', 'شراء ناجح', `تم شراء «${item.name_ar}» مقابل ${item.price} Miy`);
  ctx.out(200, { ok: true, balance: res.balance, itemId: item.id });
}, { auth: true });

getRoute('/api/inventory', async (ctx) => {
  const rows = all(
    `SELECT s.*, i.acquired_at FROM inventory i JOIN shop_items s ON s.id = i.item_id
     WHERE i.user_id = ? ORDER BY i.acquired_at DESC`, ctx.user.id
  );
  ctx.out(200, {
    ok: true,
    items: rows.map((r) => ({
      id: r.id, nameAr: r.name_ar, nameEn: r.name_en, category: r.category, color: r.color,
      rarity: r.rarity, bodyPart: r.body_part, acquiredAt: r.acquired_at,
    })),
  });
}, { auth: true });

// Equip enforces server-side ownership: the client can not wear an item it does not own.
post('/api/inventory/equip', async (ctx) => {
  const u = get('SELECT * FROM users WHERE id = ?', ctx.user.id);
  const appearance = { ...safeJson(u.appearance, {}) };
  const itemId = ctx.body.itemId ? String(ctx.body.itemId) : '';
  const slot = String(ctx.body.slot || '');
  if (!itemId) {
    if (!slot) throw badRequest('bad_slot', 'حدد الفتحة');
    appearance[slot] = '';
  } else {
    const owned = get('SELECT s.* FROM inventory i JOIN shop_items s ON s.id = i.item_id WHERE i.user_id = ? AND i.item_id = ?',
      ctx.user.id, itemId);
    if (!owned) throw badRequest('not_owned', 'لا تملك هذا العنصر');
    appearance[owned.body_part] = owned.id;
  }
  run('UPDATE users SET appearance = ? WHERE id = ?', JSON.stringify(appearance), u.id);
  ctx.out(200, { ok: true, appearance });
}, { auth: true });

getRoute('/api/economy/ledger', async (ctx) => {
  ctx.out(200, { ok: true, entries: ledgerFor(ctx.user.id, 80), balance: get('SELECT miy FROM users WHERE id = ?', ctx.user.id).miy });
}, { auth: true });

getRoute('/api/economy/summary', async (ctx) => {
  const u = get('SELECT * FROM users WHERE id = ?', ctx.user.id);
  const today = new Date(); today.setUTCHours(0, 0, 0, 0);
  const mins = get(
    'SELECT COALESCE(SUM(credited_minutes),0) AS m FROM map_sessions WHERE user_id = ? AND started_at >= ?',
    ctx.user.id, Math.floor(today.getTime() / 1000)
  ).m;
  ctx.out(200, {
    ok: true,
    balance: u.miy,
    miyPerMinute: 5,
    minutesToday: mins,
    earnedToday: mins * 5,
    dailyCapMinutes: 240,
    playSeconds: u.play_seconds,
  });
}, { auth: true });

// Admin-facing: shop catalogue management (owner only)
post('/api/admin/shop', async (ctx) => {
  const id = String(ctx.body.id || newId('item')).slice(0, 40);
  const price = clampInt(ctx.body.price, 0, 1000000, 100);
  run(
    `INSERT INTO shop_items(id,name_ar,name_en,category,price,color,rarity,body_part,active) VALUES(?,?,?,?,?,?,?,?,?)
     ON CONFLICT(id) DO UPDATE SET price=excluded.price, active=excluded.active`,
    id, String(ctx.body.nameAr || 'عنصر').slice(0, 60), String(ctx.body.nameEn || 'Item').slice(0, 60),
    String(ctx.body.category || 'shirt').slice(0, 20), price, String(ctx.body.color || '#8899ff').slice(0, 9),
    String(ctx.body.rarity || 'common').slice(0, 20), String(ctx.body.bodyPart || 'torso').slice(0, 20),
    ctx.body.active === false ? 0 : 1
  );
  ctx.out(200, { ok: true, id });
}, { auth: true, owner: true });
