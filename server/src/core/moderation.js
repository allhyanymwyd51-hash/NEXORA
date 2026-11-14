// Text moderation: profanity filter used for chat, map names, descriptions.
// Kept deliberately narrow and language-aware (Arabic + English) to avoid false positives.
const AR_BLOCKED = ['خرا', 'كلب', 'حمار', 'شرموط', 'قحبه', 'قحبة', 'زب', 'طيز', 'كس', 'عاهر', 'زانيه', 'زانية', 'لوطي', 'خول'];
const EN_BLOCKED = ['fuck', 'shit', 'bitch', 'asshole', 'bastard', 'slut', 'whore', 'nigger', 'faggot', 'retard'];
const LEET = { '0': 'o', '1': 'i', '3': 'e', '4': 'a', '5': 's', '7': 't', '@': 'a', '$': 's' };

function normalise(text) {
  return String(text)
    .toLowerCase()
    .normalize('NFKC')
    // strip Arabic diacritics and tatweel
    .replace(/[\u064B-\u065F\u0670\u0640]/g, '')
    // unify alef/yaa forms
    .replace(/[\u0622\u0623\u0625]/g, '\u0627')
    .replace(/\u0649/g, '\u064A')
    .replace(/[013457@$]/g, (c) => LEET[c] || c)
    .replace(/[^\p{L}\p{N}\s]/gu, ' ')
    .replace(/\s+/g, ' ')
    .trim();
}

export function containsProfanity(text) {
  const n = normalise(text);
  const compact = n.replace(/\s+/g, '');
  for (const w of AR_BLOCKED) if (n.includes(w)) return true;
  for (const w of EN_BLOCKED) if (n.includes(w) || compact.includes(w)) return true;
  return false;
}

const escapeRe = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

// Blanks a blocked word while keeping the original length, so the stored message
// still reads naturally and the client can render it without extra logic.
export function maskProfanity(text) {
  const src = String(text);
  let out = src;
  for (const w of [...AR_BLOCKED, ...EN_BLOCKED]) {
    const re = new RegExp(escapeRe(w), 'gi');
    out = out.replace(re, (m) => '*'.repeat(m.length));
  }
  return out;
}

export function sanitizeChat(text, maxLen = 400) {
  const clean = String(text ?? '').replace(/[\u0000-\u001F\u007F]/g, '').trim().slice(0, maxLen);
  return maskProfanity(clean);
}
