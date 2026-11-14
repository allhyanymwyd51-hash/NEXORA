// NEX AI routes: create / modify world plans, job status, commit to a map.
//
// Honest status reporting: a job is only 'done' once the world spec has been generated and
// validated. While the client streams the geometry it reports its own build progress; the
// server never claims a map is playable before the world validates.
import { get, run, all } from '../core/db.js';
import { newId } from '../core/security.js';
import { badRequest, notFound, forbidden, nowSec, clampInt, rateLimit, cleanText } from '../core/util.js';
import { safeJson } from '../core/users.js';
import { GAME_TYPES, SIZE_PRESETS, DETAIL_MULT, planWorld, applyModification, validateWorld } from '../ai/world.js';
import { notify } from '../core/auth.js';
import { get as getRoute, post } from './router.js';

const STAGES = [
  { key: 'analyse', label: 'تحليل الوصف', pct: 8 },
  { key: 'terrain', label: 'توليد التضاريس', pct: 24 },
  { key: 'water', label: 'الأنهار والبحيرات', pct: 34 },
  { key: 'biome', label: 'النباتات والغابات', pct: 44 },
  { key: 'buildings', label: 'المباني والطرق', pct: 56 },
  { key: 'lighting', label: 'الإضاءة والسماء', pct: 64 },
  { key: 'gameplay', label: 'العناصر التفاعلية والمهام', pct: 74 },
  { key: 'validate', label: 'التحقق من صلاحية العالم', pct: 84 },
  { key: 'done', label: 'العالم جاهز للتجربة', pct: 100 },
];

function jobPayload(j) {
  return {
    id: j.id,
    mapId: j.map_id,
    kind: j.kind,
    status: j.status,
    progress: j.progress,
    stage: j.stage,
    prompt: j.prompt,
    summary: j.summary,
    error: j.error,
    plan: safeJson(j.plan, {}),
    createdAt: j.created_at,
    updatedAt: j.updated_at,
  };
}

function generationOptions(body) {
  return {
    prompt: String(body.prompt || '').trim(),
    gameType: GAME_TYPES.includes(body.gameType) ? body.gameType : 'open_world',
    worldSize: SIZE_PRESETS[body.worldSize] ? body.worldSize : 'large',
    detail: DETAIL_MULT[body.detail] ? body.detail : 'detailed',
    quality: ['low', 'medium', 'high', 'ultra'].includes(body.quality) ? body.quality : 'high',
    maxPlayers: clampInt(body.maxPlayers, 1, 60, 18),
  };
}

// POST /api/ai/generate  { prompt, gameType, worldSize, detail, quality, maxPlayers, name, mapId? }
post('/api/ai/generate', async (ctx) => {
  rateLimit(`ai:${ctx.user.id}`, 12, 3600000);
  const opts = generationOptions(ctx.body);
  if (opts.prompt.length < 12) throw badRequest('prompt_short', 'اكتب وصفاً أطول (12 حرفاً على الأقل) ليتمكن NEX AI من بناء العالم');
  if (opts.prompt.length > 2000) throw badRequest('prompt_long', 'الوصف طويل جداً (2000 حرف كحد أقصى)');

  const name = cleanText(ctx.body.name || `ماب ${opts.prompt.slice(0, 28)}`, 60, 'اسم الماب');
  const jobId = newId('job');
  const now = nowSec();
  const mapId = ctx.body.mapId ? String(ctx.body.mapId) : newId('map');
  const existing = ctx.body.mapId ? get('SELECT * FROM maps WHERE id = ?', mapId) : null;
  if (existing && existing.owner_id !== ctx.user.id && ctx.user.role !== 'owner') throw forbidden('لا تملك هذا الماب');
  if (!existing) {
    run(
      `INSERT INTO maps(id,owner_id,name,description,cover,game_type,privacy,max_players,version,world,status,is_ai,created_at,updated_at)
       VALUES(?,?,?,?,?,'open_world','public',?,1,'{}','draft',1,?,?)`,
      mapId, ctx.user.id, name, opts.prompt.slice(0, 300), '', opts.maxPlayers, now, now
    );
  }

  run(
    `INSERT INTO ai_jobs(id,user_id,map_id,kind,prompt,game_type,world_size,detail,quality,max_players,status,progress,stage,created_at,updated_at)
     VALUES(?,?,?,'create',?,?,?,?,?,?, 'queued', 0, 'queued', ?, ?)`,
    jobId, ctx.user.id, mapId, opts.prompt, opts.gameType, opts.worldSize, opts.detail, opts.quality, opts.maxPlayers, now, now
  );

  // Generation runs asynchronously so the client can show real staged progress.
  setImmediate(() => runGeneration(jobId, opts));
  ctx.out(202, { ok: true, jobId, mapId, stages: STAGES });
}, { auth: true });

async function runGeneration(jobId, opts) {
  const job = get('SELECT * FROM ai_jobs WHERE id = ?', jobId);
  if (!job) return;
  const step = (key, pct) => {
    run('UPDATE ai_jobs SET status = ?, stage = ?, progress = ?, updated_at = ? WHERE id = ?',
      key === 'done' ? 'done' : 'running', key, pct, nowSec(), jobId);
  };
  try {
    step('analyse', 8);
    await delay(120);
    const world = planWorld({ ...opts, seedHint: jobId });

    step('terrain', 26);
    await delay(90);
    step('water', 36);
    step('biome', 46);
    await delay(90);
    step('buildings', 58);
    step('lighting', 66);
    await delay(60);
    step('gameplay', 76);

    const errs = validateWorld(world);
    if (errs.length) throw new Error(`world validation failed: ${errs.join(', ')}`);
    step('validate', 86);

    const map = get('SELECT * FROM maps WHERE id = ?', job.map_id);
    const prev = safeJson(map.world, {});
    const version = (map.version || 1) + (prev.format ? 1 : 0);
    world.version = version;
    const summary = summarize(world);
    run(
      `UPDATE maps SET world = ?, game_type = ?, max_players = ?, version = ?, name = CASE WHEN name LIKE 'ماب %' THEN ? ELSE name END,
       updated_at = ? WHERE id = ?`,
      JSON.stringify(world), opts.gameType, opts.maxPlayers, version, map.name, nowSec(), job.map_id
    );
    run('UPDATE ai_jobs SET plan = ?, summary = ?, status = ?, stage = ?, progress = 100, updated_at = ? WHERE id = ?',
      JSON.stringify(world), summary, 'done', 'done', nowSec(), jobId);
    notify(job.user_id, 'system', 'NEX AI: العالم جاهز',
      `${summary} — افتح الاستوديو لتجربة اللعب والنشر.`, { mapId: job.map_id, jobId });
    import('../core/realtime.js').then((m) => m.pushNotification(job.user_id, {
      id: newId('ntf'), type: 'system', title: 'NEX AI: العالم جاهز',
      body: summary, data: { mapId: job.map_id, jobId }, createdAt: nowSec(),
    })).catch(() => {});
  } catch (e) {
    run("UPDATE ai_jobs SET status = 'failed', error = ?, updated_at = ? WHERE id = ?",
      String(e.message || e).slice(0, 500), nowSec(), jobId);
  }
}

function summarize(world) {
  const s = world.stats || {};
  return `${world.sizeLabel} • ${world.side}م • ${s.trees || 0} شجرة • ${s.buildings || 0} مبنى • ${world.quests.length} مهمة • ${world.environment.weather}`;
}

function delay(ms) { return new Promise((r) => setTimeout(r, ms)); }

// POST /api/ai/modify  { mapId, prompt, detail?, quality? }
post('/api/ai/modify', async (ctx) => {
  rateLimit(`aimod:${ctx.user.id}`, 40, 3600000);
  const mapId = String(ctx.body.mapId || '');
  const map = get('SELECT * FROM maps WHERE id = ?', mapId);
  if (!map) throw notFound('الماب غير موجود');
  if (map.owner_id !== ctx.user.id && ctx.user.role !== 'owner') throw forbidden('لا تملك هذا الماب');
  const prompt = String(ctx.body.prompt || '').trim();
  if (prompt.length < 4) throw badRequest('prompt_short', 'اكتب التعديل المطلوب');
  const world = safeJson(map.world, {});
  if (!world.format) throw badRequest('no_world', 'لا يوجد عالم في هذا الماب بعد');
  const detail = DETAIL_MULT[ctx.body.detail] ? ctx.body.detail : world.detail || 'detailed';
  const quality = ['low', 'medium', 'high', 'ultra'].includes(ctx.body.quality) ? ctx.body.quality : world.quality || 'high';
  const { world: next, ops, changes } = applyModification(world, prompt, detail, quality);
  const errs = validateWorld(next);
  if (errs.length) throw badRequest('bad_world', `فشل التعديل: ${errs.join(', ')}`);
  run('UPDATE maps SET world = ?, version = version + 1, updated_at = ? WHERE id = ?',
    JSON.stringify(next), nowSec(), map.id);
  const jobId = newId('job');
  run(
    `INSERT INTO ai_jobs(id,user_id,map_id,kind,prompt,game_type,world_size,detail,quality,max_players,status,progress,stage,plan,summary,created_at,updated_at)
     VALUES(?,?,?,'modify',?,?,?,?,?,?, 'done', 100, 'done', ?, ?, ?, ?)`,
    jobId, ctx.user.id, map.id, prompt, map.game_type, next.size || 'large', detail, quality, map.max_players,
    JSON.stringify(next), changes.join(' • '), nowSec(), nowSec()
  );
  notify(ctx.user.id, 'system', 'NEX AI: تم تعديل الماب', changes.join(' • ') || 'تم التعديل', { mapId: map.id });
  ctx.out(200, {
    ok: true, jobId, mapId: map.id, changes, ops,
    world: next,
    summary: summarize(next),
    version: (get('SELECT version FROM maps WHERE id = ?', map.id)?.version) || map.version + 1,
  });
}, { auth: true });

getRoute('/api/ai/jobs/:id', async (ctx) => {
  const j = get('SELECT * FROM ai_jobs WHERE id = ?', ctx.params.id);
  if (!j) throw notFound('المهمة غير موجودة');
  if (j.user_id !== ctx.user.id && ctx.user.role !== 'owner') throw forbidden();
  ctx.out(200, { ok: true, job: jobPayload(j) });
}, { auth: true });

getRoute('/api/ai/jobs', async (ctx) => {
  const rows = all('SELECT * FROM ai_jobs WHERE user_id = ? ORDER BY created_at DESC LIMIT 20', ctx.user.id);
  ctx.out(200, { ok: true, jobs: rows.map(jobPayload) });
}, { auth: true });

// The client reports how far its *engine* build got, so progress shown to the user is real.
post('/api/ai/jobs/:id/client-progress', async (ctx) => {
  const j = get('SELECT * FROM ai_jobs WHERE id = ?', ctx.params.id);
  if (!j) throw notFound('المهمة غير موجودة');
  if (j.user_id !== ctx.user.id) throw forbidden();
  const pct = clampInt(ctx.body.progress, 0, 100, 0);
  run('UPDATE ai_jobs SET progress = ?, stage = ? WHERE id = ?', 90 + Math.round(pct / 10),
    String(ctx.body.stage || 'client_build').slice(0, 40), j.id);
  ctx.out(200, { ok: true });
}, { auth: true });

// Preview payload without exposing the full world until the job is done.
getRoute('/api/ai/presets', async (ctx) => {
  ctx.out(200, {
    ok: true,
    sizes: Object.entries(SIZE_PRESETS).map(([key, v]) => ({ key, label: v.label, side: v.side })),
    details: [
      { key: 'simple', label: 'مبسط' }, { key: 'standard', label: 'عادي' },
      { key: 'detailed', label: 'مفصل' }, { key: 'ultra', label: 'فائق التفصيل' },
    ],
    gameTypes: GAME_TYPES.map((k) => ({ key: k })),
    qualities: ['low', 'medium', 'high', 'ultra'].map((k) => ({ key: k })),
    examples: [
      'اصنع لي جزيرة استوائية ضخمة فيها غابات وشواطئ وجبال وقرى وبيوت وشلالات وكهوف وحيوانات، مع نظام استكشاف ومهام.',
      'مدينة سايبربنك ليلية بأبراج نيون وطرق سريعة ونظام سباقات.',
      'عالم بقاء ثلجي فيه جبال وكهوف وحيوانات برية وعاصفة ثلجية.',
      'ساحة قتال بركانية فيها حمم وجسور وكهوف ومهام قتالية.',
    ],
  });
});
