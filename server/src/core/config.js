// NEXORA server configuration.
// All secrets come from the environment. Nothing sensitive is ever written in source.
import { randomBytes } from 'node:crypto';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
export const ROOT = path.resolve(__dirname, '..', '..');

function loadDotEnv() {
  const p = path.join(ROOT, '.env');
  if (!fs.existsSync(p)) return;
  const txt = fs.readFileSync(p, 'utf8');
  for (const raw of txt.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const eq = line.indexOf('=');
    if (eq < 0) continue;
    const k = line.slice(0, eq).trim();
    let v = line.slice(eq + 1).trim();
    if ((v.startsWith('"') && v.endsWith('"')) || (v.startsWith("'") && v.endsWith("'"))) v = v.slice(1, -1);
    if (process.env[k] === undefined) process.env[k] = v;
  }
}
loadDotEnv();

const env = process.env;

export const config = {
  port: Number(env.PORT || 8080),
  host: env.HOST || '0.0.0.0',
  dbPath: env.DB_PATH || path.join(ROOT, 'data', 'nexora.db'),
  backupDir: env.BACKUP_DIR || path.join(ROOT, 'data', 'backups'),
  // Secret used to sign session tokens. Generated once and persisted to data/secret.key.
  secret: (() => {
    if (env.NEXORA_SECRET && env.NEXORA_SECRET.length >= 32) return env.NEXORA_SECRET;
    const keyFile = path.join(ROOT, 'data', 'secret.key');
    try {
      const s = fs.readFileSync(keyFile, 'utf8').trim();
      if (s.length >= 32) return s;
    } catch { /* not created yet */ }
    const s = randomBytes(48).toString('hex');
    fs.mkdirSync(path.dirname(keyFile), { recursive: true });
    fs.writeFileSync(keyFile, s, { mode: 0o600 });
    return s;
  })(),
  sessionDays: Number(env.SESSION_DAYS || 30),
  trustProxy: env.TRUST_PROXY === '1',
  // NEX AI provider (optional). Without a key the deterministic local planner is used.
  ai: {
    baseUrl: env.AI_BASE_URL || '',
    apiKey: env.AI_API_KEY || '',
    model: env.AI_MODEL || '',
    timeoutMs: Number(env.AI_TIMEOUT_MS || 45000),
  },
  economy: {
    miyPerMinute: 5,
    maxMinutesPerDay: 240,
    shopCurrency: 'Miy',
  },
  limits: {
    maxMapPlayers: 60,
    minMapPlayers: 1,
    maxWorldSide: 2400,
    usernameMin: 3,
    usernameMax: 20,
    passwordMin: 8,
    passwordMax: 128,
  },
  version: '1.0.0',
};

export function isConfiguredAIOffline() {
  return !config.ai.baseUrl || !config.ai.apiKey;
}
