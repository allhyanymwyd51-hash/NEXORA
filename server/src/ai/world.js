// NEXORA world planner.
//
// Turns a natural-language description into a *deterministic, engine-consumable world spec*
// (format `nexora-world/1`). The spec is what the Unreal client's WorldBuilder executes:
// heightfield parameters, biome regions, explicit object placements, roads, quests, spawns,
// lighting, audio. Nothing here is a picture: every field drives real geometry on the client.
//
// A remote LLM can enrich the *plan text* (summary, quests, feature names) when configured,
// but the structural generator below always produces a valid, playable world locally.

import crypto from 'node:crypto';

export const WORLD_FORMAT = 'nexora-world/1';
export const SIZE_PRESETS = {
  small: { side: 640, label: 'صغير', density: 0.55 },
  medium: { side: 1000, label: 'متوسط', density: 0.8 },
  large: { side: 1400, label: 'كبير', density: 1.0 },
  huge: { side: 1900, label: 'ضخم', density: 1.25 },
  aaa: { side: 2400, label: 'AAA Ultra', density: 1.6 },
};
export const DETAIL_MULT = { simple: 0.5, standard: 0.85, detailed: 1.15, ultra: 1.5 };
export const GAME_TYPES = [
  'open_world', 'life_sim', 'adventure', 'racing', 'survival', 'horror',
  'combat', 'building', 'exploration', 'social',
];
export const GAME_TYPE_LABELS = {
  open_world: 'عالم مفتوح', life_sim: 'محاكاة حياة', adventure: 'مغامرات', racing: 'سباقات',
  survival: 'بقاء', horror: 'رعب', combat: 'قتال', building: 'بناء', exploration: 'استكشاف', social: 'اجتماعي',
};

// ---------------------------------------------------------------------------
// deterministic RNG + noise
// ---------------------------------------------------------------------------
export function hashSeed(str) {
  const h = crypto.createHash('sha256').update(String(str)).digest();
  return h.readUInt32LE(0) || 1;
}

function mulberry32(seed) {
  let a = seed >>> 0;
  return function rnd() {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

const R = (rnd, a, b) => a + (b - a) * rnd();
const RI = (rnd, a, b) => Math.floor(R(rnd, a, b + 1));
const pick = (rnd, arr) => arr[Math.floor(rnd() * arr.length) % arr.length];
const round1 = (v) => Math.round(v * 10) / 10;

// ---------------------------------------------------------------------------
// prompt understanding (Arabic + English keywords, Arabic normalization)
// ---------------------------------------------------------------------------
function normalize(text) {
  return String(text || '')
    .toLowerCase()
    .normalize('NFKC')
    .replace(/[\u064B-\u065F\u0670\u0640]/g, '')
    .replace(/[\u0622\u0623\u0625]/g, '\u0627')
    .replace(/\u0649/g, '\u064A')
    .replace(/\u0629/g, '\u0647');
}

const KEYWORDS = {
  island: ['جزير', 'island', 'isle'],
  forest: ['غاب', 'شجر', 'forest', 'jungle', 'woods', 'أشجار'],
  mountain: ['جبل', 'جبال', 'mountain', 'hill', 'peak', 'قمم'],
  desert: ['صحر', 'desert', 'رمال', 'dune', 'كثبان'],
  snow: ['ثلج', 'جليد', 'snow', 'ice', 'glacier', 'winter'],
  beach: ['شاطئ', 'شواطئ', 'beach', 'shore', 'ساحل', 'sand'],
  city: ['مدين', 'city', 'metropolis', 'حضر'],
  village: ['قري', 'village', 'town', 'بلدة'],
  house: ['بيوت', 'منازل', 'houses', 'home', 'منزل', 'building'],
  cave: ['كهف', 'كهوف', 'cave', 'cavern', 'مغار'],
  waterfall: ['شلال', 'waterfall', 'falls'],
  river: ['نهر', 'أنهار', 'river', 'stream'],
  lake: ['بحير', 'برك', 'lake', 'pond'],
  sea: ['بحر', 'محيط', 'sea', 'ocean', 'water'],
  animal: ['حيوان', 'حيوانات', 'animal', 'wildlife', 'creature', 'طيور', 'birds'],
  storm: ['عاصف', 'storm', 'rain', 'مطر', 'thunder', 'رعد', 'ثلوج'],
  night: ['ليل', 'night', 'dark', 'ظلام', 'قمر'],
  fog: ['ضباب', 'fog', 'mist'],
  volcano: ['بركان', 'volcano', 'lava', 'حمم'],
  volcanoAlt: ['fire', 'نار'],
  cyber: ['سايبر', 'cyber', 'neon', 'مستقبل', 'futuristic', 'نيون'],
  horror: ['رعب', 'horror', 'scary', 'مرعب', 'مظلم'],
  race: ['سباق', 'race', 'racing', 'track', 'حلبة', 'سيارات'],
  survival: ['بقاء', 'survival', 'craft', 'نجاة'],
  combat: ['قتال', 'combat', 'fight', 'battle', 'حرب', 'arena', 'ساحة'],
  building: ['بناء', 'build', 'sandbox', 'بلوك'],
  quest: ['مهم', 'quest', 'mission', 'مهام', 'objective'],
  treasure: ['كنز', 'treasure', 'chest', 'صندوق'],
  portal: ['بواب', 'portal', 'teleport', 'انتقال'],
  crystals: ['بلور', 'crystal', 'gem', 'جواهر'],
};

export function analysePrompt(prompt) {
  const text = normalize(prompt);
  const hits = {};
  for (const [key, words] of Object.entries(KEYWORDS)) hits[key] = words.some((w) => text.includes(normalize(w)));
  // Arabic plural/derived forms that the simple substring test misses
  hits.forest = hits.forest || /اشجار|نبات|خضره/.test(text);
  hits.city = hits.city || /عمار|ابراج|شوارع/.test(text);
  const desc = String(prompt || '').trim();
  return { text, hits, length: desc.length };
}

// ---------------------------------------------------------------------------
// planner
// ---------------------------------------------------------------------------
export function planWorld(opts) {
  const prompt = String(opts.prompt || '').slice(0, 2000);
  const gameType = GAME_TYPES.includes(opts.gameType) ? opts.gameType : 'open_world';
  const sizeKey = SIZE_PRESETS[opts.worldSize] ? opts.worldSize : 'large';
  const detailKey = DETAIL_MULT[opts.detail] ? opts.detail : 'detailed';
  const quality = ['low', 'medium', 'high', 'ultra'].includes(opts.quality) ? opts.quality : 'high';
  const maxPlayers = Math.max(1, Math.min(60, Number(opts.maxPlayers) || 18));
  const preset = SIZE_PRESETS[sizeKey];
  const density = preset.density * DETAIL_MULT[detailKey];
  const seed = hashSeed(`${prompt}|${gameType}|${sizeKey}|${opts.seedHint || ''}`);
  const rnd = mulberry32(seed);
  const a = analysePrompt(prompt);
  const h = a.hits;
  const side = preset.side;
  const half = side / 2;

  // ---- theme resolution ---------------------------------------------------
  const isHorror = gameType === 'horror' || h.horror;
  const isRace = gameType === 'racing' || h.race;
  const isCombat = gameType === 'combat' || h.combat;
  const theme = {
    island: h.island || (h.sea && !h.desert),
    forest: h.forest || (!h.desert && !h.city && !h.cyber),
    mountain: h.mountain || h.snow,
    desert: h.desert,
    snow: h.snow,
    city: h.city,
    village: h.village || h.house,
    cave: h.cave,
    waterfall: h.waterfall,
    animals: h.animal,
    cyber: h.cyber,
    volcano: h.volcano,
    horror: isHorror,
    race: isRace,
    combat: isCombat,
  };

  const world = {
    format: WORLD_FORMAT,
    seed,
    prompt: prompt.slice(0, 500),
    gameType,
    gameTypeLabel: GAME_TYPE_LABELS[gameType],
    size: sizeKey,
    sizeLabel: preset.label,
    detail: detailKey,
    quality,
    side,
    maxPlayers,
    theme,
    water: {
      enabled: !!theme.island || h.sea || h.lake || h.river || theme.waterfall,
      level: theme.island ? 0 : (h.lake ? round1(R(rnd, -1, 1.5)) : -3),
      color: theme.volcano ? '#3a1d16' : (isHorror ? '#10222c' : '#1f6f8b'),
      depth: theme.island ? 'deep' : 'medium',
      waves: round1(R(rnd, 0.3, 1.1)),
      foam: quality !== 'low',
    },
    terrain: {
      baseHeight: round1(R(rnd, 2, 8)),
      roughness: round1(R(rnd, 0.6, 1.6)),
      octaves: quality === 'low' ? 3 : quality === 'medium' ? 4 : 5,
      mountainAmplitude: theme.mountain ? round1(R(rnd, 55, 140)) : round1(R(rnd, 6, 18)),
      peaks: buildPeaks(rnd, half, theme, density, h),
      basins: buildBasins(rnd, half, theme, h),
      rivers: buildRivers(rnd, half, theme, h),
      plateaus: buildPlateaus(rnd, half, theme),
      fillOcean: theme.island,
    },
    regions: buildRegions(rnd, half, theme, h, side),
    forests: buildForests(rnd, half, theme, density, h),
    rocks: buildRocks(rnd, half, theme, density),
    buildings: buildBuildings(rnd, half, theme, gameType, density, h),
    roads: [],
    waterfalls: [],
    caves: [],
    pois: [],
    spawns: [],
    animals: [],
    interactables: [],
    quests: [],
    environment: buildEnvironment(rnd, theme, gameType, h),
    audio: {
      ambient: theme.volcano ? 'volcano_loop' : theme.island ? 'beach_waves' : theme.snow ? 'snow_wind'
        : theme.forest ? 'forest_day' : theme.city ? 'city_hum' : 'open_plain',
      music: isHorror ? 'horror_theme' : isRace ? 'race_energy' : isCombat ? 'combat_tense'
        : gameType === 'social' ? 'social_lofi' : 'exploration',
      footsteps: theme.snow ? 'snow' : theme.desert ? 'sand' : theme.city ? 'concrete' : 'grass',
    },
    features: [],
    stats: {},
    createdAt: Math.floor(Date.now() / 1000),
  };

  // ---- waterfalls (need cliffs) -------------------------------------------
  if (theme.waterfall) {
    const count = h.waterfall ? RI(rnd, 1, 3) : RI(rnd, 0, 1);
    for (let i = 0; i < count; i++) {
      const peak = world.terrain.peaks[0] || { x: R(rnd, -half * 0.4, half * 0.4), z: R(rnd, -half * 0.4, half * 0.4), height: 40 };
      world.waterfalls.push({
        x: round1(peak.x + R(rnd, -60, 60)), z: round1(peak.z + R(rnd, -60, 60)),
        height: round1(Math.max(12, peak.height * 0.55)), width: round1(R(rnd, 3, 8)),
        direction: round1(R(rnd, 0, Math.PI * 2)), poolRadius: round1(R(rnd, 8, 16)),
      });
    }
  }
  if (theme.cave) {
    const count = h.cave ? RI(rnd, 2, 5) : 1;
    for (let i = 0; i < count; i++) {
      world.caves.push({
        x: round1(R(rnd, -half * 0.75, half * 0.75)), z: round1(R(rnd, -half * 0.75, half * 0.75)),
        radius: round1(R(rnd, 5, 12)), length: round1(R(rnd, 18, 55)), depth: round1(R(rnd, 6, 22)),
        lit: !isHorror && quality !== 'low',
      });
    }
  }

  // ---- points of interest --------------------------------------------------
  world.pois = buildPois(rnd, world, theme, gameType, h, half);
  world.roads = buildRoadLinks(world.pois, half);
  world.spawns = buildSpawns(rnd, world, half);
  if (theme.animals) world.animals = buildAnimals(rnd, world, half, density, theme);
  world.interactables = buildInteractables(rnd, world, theme, gameType, h, half);
  world.quests = buildQuests(rnd, world, gameType, h, theme);
  world.features = describeFeatures(world, theme, h, gameType);
  world.stats = estimateStats(world, quality);

  // Hard guard: never claim detail the tier cannot carry.
  world.stats.qualityNote = quality === 'ultra'
    ? 'إضاءة متقدمة وظلال ديناميكية وكثافة نبات عالية (تتطلب جهازاً قوياً)'
    : quality === 'high' ? 'ظلال ديناميكية وكثافة نبات متوسطة'
    : quality === 'medium' ? 'ظلال مبسطة وكثافة نبات منخفضة' : 'أداء محسّن للأجهزة الضعيفة';
  return world;
}

function buildPeaks(rnd, half, theme, density, h) {
  const peaks = [];
  if (!theme.mountain && !theme.volcano) {
    // rolling hills only
    const n = Math.round(3 * density);
    for (let i = 0; i < n; i++) {
      peaks.push({ x: round1(R(rnd, -half * 0.8, half * 0.8)), z: round1(R(rnd, -half * 0.8, half * 0.8)),
        height: round1(R(rnd, 6, 22)), radius: round1(R(rnd, 60, 150)), kind: 'hill' });
    }
    return peaks;
  }
  const ranges = theme.volcano ? 1 : RI(rnd, 2, 4);
  for (let r = 0; r < ranges; r++) {
    const cx = R(rnd, -half * 0.7, half * 0.7);
    const cz = R(rnd, -half * 0.7, half * 0.7);
    const dir = R(rnd, 0, Math.PI * 2);
    const count = RI(rnd, 3, 6);
    for (let i = 0; i < count; i++) {
      const t = (i - count / 2) * R(rnd, 55, 110);
      peaks.push({
        x: round1(cx + Math.cos(dir) * t + R(rnd, -20, 20)),
        z: round1(cz + Math.sin(dir) * t + R(rnd, -20, 20)),
        height: round1(theme.volcano ? R(rnd, 90, 170) : R(rnd, 45, 150) * (1 - Math.abs(i - count / 2) * 0.12)),
        radius: round1(R(rnd, 70, 150)),
        kind: theme.volcano ? 'volcano' : theme.snow ? 'snow_peak' : 'mountain',
      });
    }
  }
  return peaks;
}

function buildBasins(rnd, half, theme, h) {
  const out = [];
  if (h.lake || h.river) {
    const n = h.lake ? RI(rnd, 1, 3) : 1;
    for (let i = 0; i < n; i++) {
      out.push({
        x: round1(R(rnd, -half * 0.6, half * 0.6)), z: round1(R(rnd, -half * 0.6, half * 0.6)),
        depth: round1(R(rnd, 3, 14)), radius: round1(R(rnd, 30, 90)), kind: 'lake',
      });
    }
  }
  if (theme.island) {
    out.push({ x: 0, z: 0, depth: 26, radius: round1(half * 0.92), kind: 'ocean' });
  }
  return out;
}

function buildRivers(rnd, half, theme, h) {
  if (!(h.river || h.waterfall || (theme.forest && rnd() > 0.55))) return [];
  const count = h.river ? RI(rnd, 1, 2) : 1;
  const rivers = [];
  for (let i = 0; i < count; i++) {
    const points = [];
    const startX = R(rnd, -half * 0.8, half * 0.8);
    const startZ = -half * 0.9;
    const drift = R(rnd, -0.5, 0.5);
    const steps = 9;
    for (let s = 0; s <= steps; s++) {
      const t = s / steps;
      points.push([
        round1(startX + Math.sin(t * Math.PI * 1.7 + i) * half * 0.22 + drift * t * half * 0.3),
        round1(startZ + t * sideLength(half) * 0.95),
      ]);
    }
    rivers.push({ points, width: round1(R(rnd, 4, 12)), depth: round1(R(rnd, 1.5, 5)), flow: round1(R(rnd, 0.4, 1)) });
  }
  return rivers;
}

function sideLength(half) { return half * 2; }

function buildPlateaus(rnd, half, theme) {
  if (!theme.city) return [];
  const out = [];
  const n = RI(rnd, 1, 2);
  for (let i = 0; i < n; i++) {
    out.push({
      x: round1(R(rnd, -half * 0.5, half * 0.5)), z: round1(R(rnd, -half * 0.5, half * 0.5)),
      radius: round1(R(rnd, 90, 180)), height: round1(R(rnd, 2, 9)),
    });
  }
  return out;
}

function buildRegions(rnd, half, theme, h, side) {
  const regions = [];
  const add = (type, x, z, radius, extra = {}) => regions.push({ type, x: round1(x), z: round1(z), radius: round1(radius), ...extra });
  if (theme.island) {
    const n = h.forest ? RI(rnd, 2, 4) : RI(rnd, 1, 2);
    for (let i = 0; i < n; i++) add('jungle', R(rnd, -half * 0.55, half * 0.55), R(rnd, -half * 0.55, half * 0.55), R(rnd, 90, 210));
    add('beach', 0, 0, half * 0.98, { ring: true, width: 26 });
    if (h.mountain) add('mountain', R(rnd, -half * 0.3, half * 0.3), R(rnd, -half * 0.3, half * 0.3), R(rnd, 110, 200));
  } else if (theme.desert) {
    add('desert', 0, 0, half * 1.2);
    if (h.forest) add('oasis', R(rnd, -half * 0.4, half * 0.4), R(rnd, -half * 0.4, half * 0.4), R(rnd, 45, 90));
  } else if (theme.snow) {
    add('snow', 0, 0, half * 1.2);
    if (h.forest) add('snow_forest', R(rnd, -half * 0.5, half * 0.5), R(rnd, -half * 0.5, half * 0.5), R(rnd, 100, 220));
  } else {
    add('grass', 0, 0, half * 1.3);
    if (theme.forest) for (let i = 0; i < RI(rnd, 1, 3); i++) {
      add('forest', R(rnd, -half * 0.6, half * 0.6), R(rnd, -half * 0.6, half * 0.6), R(rnd, 90, 220));
    }
    if (theme.city) add('urban', 0, 0, R(rnd, 140, 240));
  }
  if (theme.volcano) add('volcanic', regions[0]?.x || 0, regions[0]?.z || 0, half * 0.8);
  if (theme.cyber) add('neon_district', R(rnd, -half * 0.4, half * 0.4), R(rnd, -half * 0.4, half * 0.4), R(rnd, 120, 220));
  return regions;
}

function buildForests(rnd, half, theme, density, h) {
  if (!theme.forest && !h.forest) return [];
  const out = [];
  const clusters = Math.round(RI(rnd, 4, 9) * density);
  for (let i = 0; i < clusters; i++) {
    const x = R(rnd, -half * 0.85, half * 0.85);
    const z = R(rnd, -half * 0.85, half * 0.85);
    out.push({
      x: round1(x), z: round1(z),
      radius: round1(R(rnd, 40, 150)),
      count: Math.round(R(rnd, 25, 110) * density),
      species: theme.snow ? 'pine' : theme.island ? pick(rnd, ['palm', 'broadleaf']) : theme.desert ? 'palm'
        : pick(rnd, ['broadleaf', 'pine', 'broadleaf']),
      height: round1(R(rnd, 6, 15)),
      undergrowth: Math.round(R(rnd, 30, 120) * density),
    });
  }
  return out;
}

function buildRocks(rnd, half, theme, density) {
  const out = [];
  const n = Math.round(RI(rnd, 8, 18) * density);
  for (let i = 0; i < n; i++) {
    out.push({
      x: round1(R(rnd, -half * 0.92, half * 0.92)), z: round1(R(rnd, -half * 0.92, half * 0.92)),
      scale: round1(R(rnd, 0.6, 3.4)), count: RI(rnd, 2, 9),
      kind: theme.snow ? 'snow_rock' : theme.desert ? 'sandstone' : 'granite',
    });
  }
  return out;
}

function buildBuildings(rnd, half, theme, gameType, density, h) {
  const out = [];
  const addBuilding = (x, z, w, d, ht, style) => out.push({
    x: round1(x), z: round1(z), w: round1(w), d: round1(d), h: round1(ht),
    rot: round1(R(rnd, -Math.PI, Math.PI)), style,
    roof: style === 'modern' ? 'flat' : pick(rnd, ['gable', 'hip', 'flat']),
    floors: Math.max(1, Math.round(ht / 3.2)),
  });
  if (theme.city) {
    const blocks = Math.round(RI(rnd, 3, 6) * density) + 2;
    for (let b = 0; b < blocks; b++) {
      const bcx = R(rnd, -half * 0.55, half * 0.55);
      const bcz = R(rnd, -half * 0.55, half * 0.55);
      const blockSize = R(rnd, 60, 130);
      const perSide = RI(rnd, 2, 4);
      for (let i = 0; i < perSide; i++) {
        for (let j = 0; j < perSide; j++) {
          if (rnd() < 0.18) continue; // plazas / parks
          const w = R(rnd, 8, 16), d = R(rnd, 8, 16);
          const x = bcx + (i - perSide / 2) * (blockSize / perSide);
          const z = bcz + (j - perSide / 2) * (blockSize / perSide);
          const tall = rnd() < 0.35;
          addBuilding(x, z, w, d, tall ? R(rnd, 18, 52) : R(rnd, 5, 12), tall ? 'modern' : 'classic');
        }
      }
    }
  }
  if (theme.village) {
    const villages = RI(rnd, 2, 4);
    for (let v = 0; v < villages; v++) {
      const vx = R(rnd, -half * 0.75, half * 0.75), vz = R(rnd, -half * 0.75, half * 0.75);
      const houses = RI(rnd, 6, 16);
      for (let i = 0; i < houses; i++) {
        const ang = R(rnd, 0, Math.PI * 2), rad = R(rnd, 8, 55);
        addBuilding(vx + Math.cos(ang) * rad, vz + Math.sin(ang) * rad, R(rnd, 5, 9), R(rnd, 5, 9), R(rnd, 3, 6),
          theme.island ? 'tropical' : theme.snow ? 'alpine' : 'rural');
      }
      // a landmark: well / watchtower / shrine
      addBuilding(vx, vz, 4, 4, RI(rnd, 6, 12), 'landmark');
    }
  }
  if (gameType === 'combat') {
    const arenas = 1;
    for (let i = 0; i < arenas; i++) {
      addBuilding(R(rnd, -half * 0.3, half * 0.3), R(rnd, -half * 0.3, half * 0.3), 34, 34, 8, 'arena');
    }
  }
  if (h.cyber && !out.length) {
    const n = Math.round(20 * density);
    for (let i = 0; i < n; i++) addBuilding(R(rnd, -half * 0.5, half * 0.5), R(rnd, -half * 0.5, half * 0.5),
      R(rnd, 10, 20), R(rnd, 10, 20), R(rnd, 20, 70), 'modern');
  }
  return out.slice(0, 700);
}

function buildPois(rnd, world, theme, gameType, h, half) {
  const pois = [];
  const add = (type, x, z, name) => pois.push({ type, x: round1(x), z: round1(z), name });
  if (theme.city) add('city_center', 0, 0, 'مركز المدينة');
  for (const f of world.forests.slice(0, 3)) add('forest', f.x, f.z, 'الغابة');
  for (const w of world.waterfalls.slice(0, 2)) add('waterfall', w.x, w.z, 'الشلال');
  for (const c of world.caves.slice(0, 3)) add('cave', c.x, c.z, 'الكهف');
  for (const p of world.terrain.peaks.filter((p) => p.height > 40).slice(0, 3)) add('peak', p.x, p.z, 'القمة');
  if (world.water.enabled) {
    const ang = R(rnd, 0, Math.PI * 2);
    add('harbor', Math.cos(ang) * half * 0.6, Math.sin(ang) * half * 0.6, 'الميناء');
  }
  if (gameType === 'racing') {
    for (let i = 0; i < 4; i++) add('checkpoint', R(rnd, -half * 0.6, half * 0.6), R(rnd, -half * 0.6, half * 0.6), `نقطة ${i + 1}`);
  }
  if (theme.volcano) add('volcano', world.terrain.peaks[0]?.x || 0, world.terrain.peaks[0]?.z || 0, 'البركان');
  add('spawn', 0, half * 0.55, 'نقطة البداية');
  if (!pois.length) add('camp', 0, 0, 'المخيم');
  return pois;
}

function buildRoadLinks(pois, half) {
  const roads = [];
  const used = pois.filter((p) => p.type !== 'peak');
  for (let i = 0; i + 1 < used.length && i < 6; i++) {
    const a = used[i], b = used[i + 1];
    const steps = 5;
    const points = [];
    for (let s = 0; s <= steps; s++) {
      const t = s / steps;
      // slight curve so roads are not dead-straight
      const bend = Math.sin(t * Math.PI) * 0.12;
      points.push([
        round1(a.x + (b.x - a.x) * t + (b.z - a.z) * bend),
        round1(a.z + (b.z - a.z) * t - (b.x - a.x) * bend),
      ]);
    }
    roads.push({ points, width: 5.5, kind: 'dirt', smooth: true });
  }
  return roads;
}

function buildSpawns(rnd, world, half) {
  const spawns = [];
  const main = world.pois.find((p) => p.type === 'spawn') || { x: 0, z: 0 };
  spawns.push({ x: main.x, z: main.z, facing: 0 });
  const extra = 5;
  for (let i = 0; i < extra; i++) {
    const ang = (i / extra) * Math.PI * 2;
    spawns.push({
      x: round1(main.x + Math.cos(ang) * R(rnd, 3, 7)),
      z: round1(main.z + Math.sin(ang) * R(rnd, 3, 7)),
      facing: round1(ang),
    });
  }
  return spawns;
}

function buildAnimals(rnd, world, half, density, theme) {
  const species = theme.snow ? ['deer', 'wolf', 'rabbit']
    : theme.desert ? ['camel', 'lizard', 'falcon']
    : theme.island ? ['parrot', 'monkey', 'turtle']
    : theme.city ? ['pigeon', 'cat', 'dog']
    : ['deer', 'boar', 'rabbit', 'bird'];
  const out = [];
  const herds = Math.round(RI(rnd, 3, 7) * density);
  for (let i = 0; i < herds; i++) {
    out.push({
      type: pick(rnd, species),
      x: round1(R(rnd, -half * 0.8, half * 0.8)), z: round1(R(rnd, -half * 0.8, half * 0.8)),
      count: RI(rnd, 2, 7), radius: round1(R(rnd, 15, 40)), roam: true, passive: rnd() > 0.25,
    });
  }
  return out;
}

function buildInteractables(rnd, world, theme, gameType, h, half) {
  const out = [];
  const add = (type, label, x, z, extra = {}) => out.push({ type, label, x: round1(x), z: round1(z), ...extra });
  if (h.treasure || gameType === 'adventure' || gameType === 'exploration') {
    for (let i = 0; i < RI(rnd, 3, 6); i++) {
      add('chest', `صندوق ${i + 1}`, R(rnd, -half * 0.8, half * 0.8), R(rnd, -half * 0.8, half * 0.8), { reward: 25 });
    }
  }
  if (h.portal) {
    for (let i = 0; i < 2; i++) add('portal', `بوابة ${i + 1}`, R(rnd, -half * 0.5, half * 0.5), R(rnd, -half * 0.5, half * 0.5));
  }
  if (h.crystals) {
    for (let i = 0; i < RI(rnd, 4, 9); i++) add('crystal', 'بلورة طاقة', R(rnd, -half * 0.8, half * 0.8), R(rnd, -half * 0.8, half * 0.8));
  }
  if (theme.volcano) add('lava_vent', 'فتحة حمم', world.terrain.peaks[0]?.x || 0, world.terrain.peaks[0]?.z || 0, { hazard: true });
  if (theme.race) add('start_line', 'خط البداية', 0, 0, { width: 12 });
  for (const q of world.quests) {
    if (q.marker) add('quest_marker', q.title, q.marker.x, q.marker.z, { questId: q.id });
  }
  return out.slice(0, 60);
}

function buildQuests(rnd, world, gameType, h, theme) {
  const quests = [];
  const poi = (i) => world.pois[i % Math.max(1, world.pois.length)] || { x: 0, z: 0, name: 'المكان' };
  const tag = () => Math.random().toString(36).slice(2, 7); // only used for ids, not gameplay randomness
  if (world.pois.some((p) => p.type === 'cave')) {
    quests.push({
      id: `q_explore_${tag()}`, title: 'استكشف الكهف', marker: poi(world.pois.findIndex((p) => p.type === 'cave')),
      description: 'ادخل الكهف واعثر على البلورة المفقودة.',
      objectives: [{ type: 'reach', target: 'cave', count: 1, label: 'ادخل الكهف' },
        { type: 'collect', target: 'crystal', count: 1, label: 'اجمع البلورة' }],
      reward: { miy: 40, xp: 25 },
    });
  }
  if (world.waterfalls.length) {
    quests.push({
      id: `q_water_${tag()}`, title: 'قمة الشلال', marker: world.waterfalls[0],
      description: 'اصعد إلى أعلى الشلال والتقط صورة المعلم.',
      objectives: [{ type: 'reach', target: 'waterfall', count: 1, label: 'اصعد إلى الشلال' }],
      reward: { miy: 30, xp: 20 },
    });
  }
  if (world.forests.length) {
    quests.push({
      id: `q_wild_${tag()}`, title: 'مراقبة الحياة البرية', marker: world.forests[0],
      description: 'اقترب من الحيوانات دون أن تهرب.',
      objectives: [{ type: 'observe', target: 'animal', count: 3, label: 'راقب 3 حيوانات' }],
      reward: { miy: 35, xp: 15 },
    });
  }
  if (gameType === 'racing') {
    quests.push({
      id: `q_race_${tag()}`, title: 'أسرع لفة', marker: poi(0),
      description: 'أكمل مسار السباق في أقل من دقيقتين.',
      objectives: [{ type: 'finish', target: 'checkpoints', count: 4, label: 'اجتز 4 نقاط' }],
      reward: { miy: 60, xp: 40 },
    });
  }
  if (gameType === 'combat') {
    quests.push({
      id: `q_arena_${tag()}`, title: 'سيد الساحة', marker: poi(0),
      description: 'اهزم خمسة خصوم في الساحة.',
      objectives: [{ type: 'defeat', target: 'enemy', count: 5, label: 'اهزم 5 خصوم' }],
      reward: { miy: 70, xp: 50 },
    });
  }
  if (gameType === 'building') {
    quests.push({
      id: `q_build_${tag()}`, title: 'أول بيت', marker: poi(0),
      description: 'ابنِ بيتاً من 20 قطعة على الأقل.',
      objectives: [{ type: 'build', target: 'blocks', count: 20, label: 'ضع 20 قطعة' }],
      reward: { miy: 50, xp: 30 },
    });
  }
  if (!quests.length) {
    quests.push({
      id: `q_home_${tag()}`, title: 'استكشف العالم', marker: poi(0),
      description: 'تعرّف على العالم واستكشف أول نقطة اهتمام.',
      objectives: [{ type: 'reach', target: 'poi', count: 1, label: 'زر نقطة اهتمام' }],
      reward: { miy: 25, xp: 10 },
    });
  }
  return quests.slice(0, 6);
}

function buildEnvironment(rnd, theme, gameType, h) {
  const storm = h.storm;
  const night = h.night || gameType === 'horror';
  return {
    timeOfDay: night ? round1(R(rnd, 20, 23)) : round1(R(rnd, 7.5, 16.5)),
    sunAzimuth: round1(R(rnd, 0, 360)),
    sunElevation: night ? 8 : round1(R(rnd, 28, 66)),
    skyTop: theme.horror ? '#070a12' : night ? '#050716' : theme.island ? '#2a86d6' : theme.snow ? '#9fc4e8' : theme.desert ? '#e8b56b' : '#3f7fd6',
    skyHorizon: theme.horror ? '#161b28' : night ? '#131a3a' : theme.island ? '#a7e4ef' : theme.snow ? '#dfeaf6' : theme.desert ? '#f3d9a8' : '#bcd6f0',
    fogColor: theme.horror ? '#0b0f18' : theme.snow ? '#dfeaf6' : '#b8cbe0',
    fogDensity: storm ? 0.045 : h.fog ? 0.03 : 0.006,
    weather: storm ? 'storm' : theme.snow ? 'snow' : theme.horror ? 'eerie' : 'clear',
    wind: storm ? 1.0 : round1(R(rnd, 0.1, 0.5)),
    rain: storm,
    lightning: storm && !theme.snow,
    exposure: night ? 1.35 : 1.0,
    ambientColor: theme.horror ? '#223047' : night ? '#3a4a80' : '#d8e6ff',
  };
}

function describeFeatures(world, theme, h, gameType) {
  const f = [];
  if (world.terrain.peaks.some((p) => p.kind === 'mountain' || p.kind === 'snow_peak' || p.kind === 'volcano')) f.push('جبال ووديان');
  if (world.forests.length) f.push(`${world.forests.length} منطقة غابات`);
  if (world.buildings.length) f.push(`${world.buildings.length} مبنى / منشأة`);
  if (world.roads.length) f.push('شبكة طرق');
  if (world.terrain.rivers.length) f.push('أنهار');
  if (world.terrain.basins.some((b) => b.kind === 'lake')) f.push('بحيرات');
  if (world.water.enabled) f.push('مسطح مائي');
  if (world.waterfalls.length) f.push(`${world.waterfalls.length} شلال`);
  if (world.caves.length) f.push(`${world.caves.length} كهف`);
  if (world.animals.length) f.push('حيوانات متحركة');
  if (world.interactables.length) f.push('عناصر تفاعلية');
  f.push(`${world.quests.length} مهمة`);
  f.push(`${world.spawns.length} نقطة ظهور`);
  f.push(`طقس: ${world.environment.weather}`);
  return f;
}

function estimateStats(world, quality) {
  const treeCount = world.forests.reduce((s, f) => s + f.count, 0);
  const rockCount = world.rocks.reduce((s, r) => s + r.count, 0);
  const buildingTris = world.buildings.length * 120;
  const terrainTris = Math.round((world.side / 16) * (world.side / 16) * 2 * (quality === 'low' ? 0.25 : 1));
  const objects = treeCount + rockCount + world.buildings.length + world.animals.reduce((s, a) => s + a.count, 0)
    + world.interactables.length;
  return {
    trees: treeCount,
    rocks: rockCount,
    buildings: world.buildings.length,
    animals: world.animals.reduce((s, a) => s + a.count, 0),
    interactables: world.interactables.length,
    objects,
    triangles: terrainTris + buildingTris + treeCount * 40 + rockCount * 24,
    side: world.side,
  };
}

// ---------------------------------------------------------------------------
// "modify with AI": apply instructions to an existing world without destroying it
// ---------------------------------------------------------------------------
const MODIFY_OPS = [
  { re: /مدين|مدينه|city/i, op: 'add_city' },
  { re: /قري|village|بلده/i, op: 'add_village' },
  { re: /غاب|شجر|forest/i, op: 'add_forest' },
  { re: /حيوان|animal|بحيرات?|lake/i, op: 'add_animals' },
  { re: /طقس|عاصف|storm|مطر/i, op: 'weather_storm' },
  { re: /ليل|night/i, op: 'time_night' },
  { re: /نهار|شمس|day/i, op: 'time_day' },
  { re: /كهف|cave/i, op: 'add_cave' },
  { re: /شلال|waterfall/i, op: 'add_waterfall' },
  { re: /عميق|أعمق|deep/i, op: 'deeper_sea' },
  { re: /جبل|mountain/i, op: 'add_mountain' },
  { re: /مدن اكثر|اكبر|bigger|توسيع/i, op: 'expand' },
  { re: /مباني واقع|buildings/i, op: 'add_buildings' },
  { re: /مسار|سباق|race/i, op: 'add_race' },
  { re: /كنز|treasure/i, op: 'add_treasure' },
];

export function planModification(world, prompt) {
  const text = normalize(prompt);
  const ops = [];
  for (const m of MODIFY_OPS) if (m.re.test(text)) ops.push(m.op);
  return { ops: [...new Set(ops)], text };
}

export function applyModification(world, prompt, detail = 'detailed', quality = 'high') {
  const { ops } = planModification(world, prompt);
  if (!ops.length) {
    // Unrecognised instruction: still attempt a safe generic enrichment, never a no-op lie.
    ops.push('add_forest');
  }
  const next = JSON.parse(JSON.stringify(world));
  const seed = hashSeed(`${next.seed}:${prompt}:${next.version || 1}:${Date.now()}`);
  const rnd = mulberry32(seed);
  const half = next.side / 2;
  const density = DETAIL_MULT[detail] || 1;
  const changes = [];

  for (const op of ops) {
    if (op === 'add_city') {
      const cx = R(rnd, -half * 0.55, half * 0.55), cz = R(rnd, -half * 0.55, half * 0.55);
      let n = 0;
      for (let i = 0; i < 6; i++) {
        for (let j = 0; j < 6; j++) {
          if (rnd() < 0.2) continue;
          const tall = rnd() < 0.3;
          next.buildings.push({
            x: round1(cx + (i - 3) * 26 + R(rnd, -5, 5)), z: round1(cz + (j - 3) * 26 + R(rnd, -5, 5)),
            w: round1(R(rnd, 8, 16)), d: round1(R(rnd, 8, 16)), h: round1(tall ? R(rnd, 18, 48) : R(rnd, 6, 12)),
            rot: round1(R(rnd, -3.14, 3.14)), style: tall ? 'modern' : 'classic',
            roof: tall ? 'flat' : pick(rnd, ['gable', 'hip']), floors: tall ? 8 : 2,
          });
          n++;
        }
      }
      next.pois.push({ type: 'city_center', x: round1(cx), z: round1(cz), name: 'مدينة جديدة' });
      changes.push(`أضيفت مدينة بـ ${n} مبنى`);
    } else if (op === 'add_village') {
      const vx = R(rnd, -half * 0.7, half * 0.7), vz = R(rnd, -half * 0.7, half * 0.7);
      for (let i = 0; i < RI(rnd, 8, 16); i++) {
        const ang = R(rnd, 0, 6.28), rad = R(rnd, 6, 45);
        next.buildings.push({
          x: round1(vx + Math.cos(ang) * rad), z: round1(vz + Math.sin(ang) * rad),
          w: round1(R(rnd, 5, 9)), d: round1(R(rnd, 5, 9)), h: round1(R(rnd, 3, 6)),
          rot: round1(R(rnd, -3.14, 3.14)), style: 'rural', roof: 'gable', floors: 1,
        });
      }
      next.pois.push({ type: 'village', x: round1(vx), z: round1(vz), name: 'قرية جديدة' });
      changes.push('أضيفت قرية كاملة بمنازلها');
    } else if (op === 'add_forest') {
      const f = {
        x: round1(R(rnd, -half * 0.8, half * 0.8)), z: round1(R(rnd, -half * 0.8, half * 0.8)),
        radius: round1(R(rnd, 60, 160)), count: Math.round(R(rnd, 60, 160) * density),
        species: pick(rnd, ['broadleaf', 'pine', 'palm']), height: round1(R(rnd, 6, 14)),
        undergrowth: Math.round(R(rnd, 60, 160) * density),
      };
      next.forests.push(f);
      next.regions.push({ type: 'forest', x: f.x, z: f.z, radius: f.radius });
      changes.push(`أضيفت غابة كثيفة (${f.count} شجرة)`);
    } else if (op === 'add_animals') {
      for (let i = 0; i < 3; i++) {
        next.animals.push({
          type: pick(rnd, ['deer', 'boar', 'rabbit', 'bird', 'wolf']),
          x: round1(R(rnd, -half * 0.8, half * 0.8)), z: round1(R(rnd, -half * 0.8, half * 0.8)),
          count: RI(rnd, 3, 7), radius: round1(R(rnd, 15, 35)), roam: true, passive: rnd() > 0.3,
        });
      }
      changes.push('أضيفت مجموعات حيوانات جديدة');
    } else if (op === 'weather_storm') {
      next.environment.weather = 'storm';
      next.environment.fogDensity = 0.045;
      next.environment.rain = true;
      next.environment.lightning = true;
      next.environment.wind = 1.0;
      changes.push('تحول الطقس إلى عاصفة رعدية');
    } else if (op === 'time_night') {
      next.environment.timeOfDay = 22;
      next.environment.sunElevation = 6;
      next.environment.skyTop = '#050716';
      next.environment.skyHorizon = '#131a3a';
      next.environment.exposure = 1.4;
      changes.push('تحول الوقت إلى الليل');
    } else if (op === 'time_day') {
      next.environment.timeOfDay = 12;
      next.environment.sunElevation = 58;
      next.environment.exposure = 1.0;
      changes.push('تحول الوقت إلى النهار');
    } else if (op === 'add_cave') {
      const c = {
        x: round1(R(rnd, -half * 0.75, half * 0.75)), z: round1(R(rnd, -half * 0.75, half * 0.75)),
        radius: round1(R(rnd, 6, 14)), length: round1(R(rnd, 25, 70)), depth: round1(R(rnd, 8, 26)), lit: quality !== 'low',
      };
      next.caves.push(c);
      next.pois.push({ type: 'cave', x: c.x, z: c.z, name: 'كهف جديد' });
      changes.push('أُنشئ كهف ضخم قابل للاستكشاف');
    } else if (op === 'add_waterfall') {
      const w = {
        x: round1(R(rnd, -half * 0.6, half * 0.6)), z: round1(R(rnd, -half * 0.6, half * 0.6)),
        height: round1(R(rnd, 15, 45)), width: round1(R(rnd, 3, 8)), direction: round1(R(rnd, 0, 6.28)),
        poolRadius: round1(R(rnd, 8, 16)),
      };
      next.waterfalls.push(w);
      next.water.enabled = true;
      changes.push('أضيف شلال وبحيرة أسفله');
    } else if (op === 'deeper_sea') {
      next.water.enabled = true;
      next.water.depth = 'deep';
      next.water.level = round1((next.water.level || 0) + 1.5);
      const ocean = next.terrain.basins.find((b) => b.kind === 'ocean');
      if (ocean) ocean.depth = round1(ocean.depth + 12);
      else next.terrain.basins.push({ x: 0, z: 0, depth: 30, radius: half * 0.9, kind: 'ocean' });
      next.terrain.fillOcean = true;
      changes.push('تم تعميق البحر وإضافة مناطق غوص');
    } else if (op === 'add_mountain') {
      for (let i = 0; i < RI(rnd, 2, 4); i++) {
        next.terrain.peaks.push({
          x: round1(R(rnd, -half * 0.6, half * 0.6)), z: round1(R(rnd, -half * 0.6, half * 0.6)),
          height: round1(R(rnd, 60, 150)), radius: round1(R(rnd, 60, 130)), kind: 'mountain',
        });
      }
      changes.push('أضيفت سلسلة جبال جديدة');
    } else if (op === 'expand') {
      next.side = Math.min(2400, Math.round(next.side * 1.25));
      next.terrain.mountainAmplitude = round1(next.terrain.mountainAmplitude * 1.1);
      changes.push(`تم توسيع العالم إلى ${next.side} متر`);
    } else if (op === 'add_buildings') {
      const cx = R(rnd, -half * 0.5, half * 0.5), cz = R(rnd, -half * 0.5, half * 0.5);
      for (let i = 0; i < 14; i++) {
        next.buildings.push({
          x: round1(cx + R(rnd, -50, 50)), z: round1(cz + R(rnd, -50, 50)),
          w: round1(R(rnd, 7, 14)), d: round1(R(rnd, 7, 14)), h: round1(R(rnd, 4, 20)),
          rot: round1(R(rnd, -3.14, 3.14)), style: pick(rnd, ['modern', 'classic']), roof: 'gable', floors: 2,
        });
      }
      changes.push('أضيفت مباني جديدة بتصاميم متنوعة');
    } else if (op === 'add_race') {
      const pts = [];
      for (let i = 0; i < 6; i++) {
        pts.push([round1(R(rnd, -half * 0.6, half * 0.6)), round1(R(rnd, -half * 0.6, half * 0.6))]);
      }
      pts.push(pts[0]);
      next.roads.push({ points: pts, width: 9, kind: 'asphalt', smooth: true, race: true });
      next.pois.push({ type: 'checkpoint', x: pts[0][0], z: pts[0][1], name: 'حلبة سباق' });
      changes.push('أضيف مسار سباق مغلق');
    } else if (op === 'add_treasure') {
      for (let i = 0; i < 4; i++) {
        next.interactables.push({
          type: 'chest', label: `صندوق ${i + 1}`, x: round1(R(rnd, -half * 0.8, half * 0.8)),
          z: round1(R(rnd, -half * 0.8, half * 0.8)), reward: 25,
        });
      }
      changes.push('أضيفت صناديق كنوز');
    }
  }

  next.version = (world.version || 1) + 1;
  next.modifiedAt = Math.floor(Date.now() / 1000);
  next.features = describeFeatures(next, next.theme, {}, next.gameType);
  next.stats = estimateStats(next, next.quality || quality);
  next.buildings = next.buildings.slice(0, 900);
  return { world: next, ops, changes };
}

// ---------------------------------------------------------------------------
// validation — a published world must be structurally sound for the client builder
// ---------------------------------------------------------------------------
export function validateWorld(w) {
  const errors = [];
  if (!w || typeof w !== 'object') return ['world is not an object'];
  if (w.format !== WORLD_FORMAT) errors.push('unknown format');
  if (!(w.side >= 200 && w.side <= 2400)) errors.push('invalid side');
  if (!Array.isArray(w.spawns) || !w.spawns.length) errors.push('no spawn points');
  if (!w.terrain || !Array.isArray(w.terrain.peaks)) errors.push('missing terrain');
  if (!Array.isArray(w.buildings)) errors.push('missing buildings array');
  if (!Array.isArray(w.quests)) errors.push('missing quests array');
  if (!w.environment) errors.push('missing environment');
  return errors;
}
