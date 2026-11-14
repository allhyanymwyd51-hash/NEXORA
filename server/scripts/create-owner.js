#!/usr/bin/env node
// Creates (or resets) the OWNER account `devmoyed`.
//
// The credentials are NEVER stored in source, never printed, and never shipped to the client.
// Provide them once through the environment:
//   NEXORA_OWNER_EMAIL=... NEXORA_OWNER_PASSWORD=... npm run owner
// The password is hashed (scrypt) before it touches the database.
import readline from 'node:readline';
import { openDb, get, run } from '../src/core/db.js';
import { hashPassword, newId } from '../src/core/security.js';
import { audit } from '../src/core/auth.js';
import { nowSec } from '../src/core/util.js';

openDb();

const OWNER_USERNAME = 'devmoyed';

function prompt(question, { hidden = false } = {}) {
  return new Promise((resolve) => {
    const rl = readline.createInterface({ input: process.stdin, output: process.stdout, terminal: true });
    if (hidden) {
      rl._writeToOutput = (s) => { if (s.includes(question)) rl.output.write(s); else rl.output.write('*'); };
    }
    rl.question(question, (answer) => { rl.close(); process.stdout.write('\n'); resolve(answer.trim()); });
  });
}

async function main() {
  let email = process.env.NEXORA_OWNER_EMAIL;
  let password = process.env.NEXORA_OWNER_PASSWORD;

  if (!email || !password) {
    if (!process.stdin.isTTY) {
      console.error('[owner] NEXORA_OWNER_EMAIL and NEXORA_OWNER_PASSWORD must be set in non-interactive mode.');
      process.exit(1);
    }
    console.log('NEXORA — owner account setup (credentials are hashed, never stored in plaintext)');
    email = email || await prompt('Owner email: ');
    password = password || await prompt('Owner password (min 12 chars): ', { hidden: true });
  }

  if (!/^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/.test(email)) { console.error('[owner] invalid email'); process.exit(1); }
  if (password.length < 12) { console.error('[owner] password must be at least 12 characters'); process.exit(1); }
  if (!/[0-9]/.test(password) || !/[A-Za-z]/.test(password)) {
    console.error('[owner] password must contain letters and digits'); process.exit(1);
  }

  const existingOwner = get('SELECT * FROM users WHERE role = ? LIMIT 1', 'owner');
  const existingName = get('SELECT * FROM users WHERE username = ?', OWNER_USERNAME);
  const conflictUser = existingName && existingName.role !== 'owner' ? existingName : null;
  if (conflictUser) {
    console.error(`[owner] username ${OWNER_USERNAME} is already used by a non-owner account.`);
    process.exit(1);
  }

  const hash = hashPassword(password);
  if (existingOwner) {
    run('UPDATE users SET password_hash = ?, email = ?, role = ?, banned = 0 WHERE id = ?',
      hash, email.toLowerCase(), 'owner', existingOwner.id);
    run('DELETE FROM sessions WHERE user_id = ?', existingOwner.id);
    audit(existingOwner.id, 'owner.credentials_rotated', existingOwner.id, {}, 'cli');
    console.log(`[owner] credentials rotated for ${OWNER_USERNAME}. All sessions were revoked.`);
  } else {
    const id = newId('usr');
    run(
      `INSERT INTO users(id,username,email,password_hash,birth_date,avatar,role,appearance,settings,created_at)
       VALUES(?,?,?,?,?,?,?,?,?,?)`,
      id, OWNER_USERNAME, email.toLowerCase(), hash, '1990-01-01', '', 'owner',
      JSON.stringify({ skin: '#c98e6a', hair: '#241a16', shirt: '#7c4dff', pants: '#1b1e2b', shoes: '#e6e9f2' }),
      JSON.stringify({ language: 'ar', quality: 'auto' }), nowSec()
    );
    audit(id, 'owner.created', id, {}, 'cli');
    console.log(`[owner] owner account created: ${OWNER_USERNAME}`);
  }
  console.log('[owner] done. No password has been written to disk in plaintext or logged.');
  process.exit(0);
}

main().catch((e) => { console.error('[owner]', e.message); process.exit(1); });
