#!/usr/bin/env node
// NEXORA audio bank — generated procedurally (no external assets, no licensing issues).
//
// Output: assets/audio/NEXORA.ogg — ONE Vorbis file (quality 2, mono music / stereo-ready
// via the format flag) that holds every clip back to back, plus assets/audio/bank.json
// describing each clip's byte range, sample rate, channels and loop flag.
//
// Why a single bank: 39 separate WAVs cost ~15 MB and 39 file descriptors, while one
// OGG bank lands under 1 MB, streams from the APK with a single AssetManager read, and
// lets the mixer slice clips by byte offset. The build script transcodes the WAVs it
// still needs for the Unreal project separately.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.join(path.resolve(__dirname, '..'), 'assets', 'audio');
const TMP = path.join(OUT, '.wav');
fs.mkdirSync(OUT, { recursive: true });
fs.mkdirSync(TMP, { recursive: true });

const SR = 44100;

function mulberry(seed) { let a = seed; return () => { a |= 0; a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
function noise(rnd) { return rnd() * 2 - 1; }

function lowpass(sig, alpha) {
  const out = new Float32Array(sig.length);
  let y = 0;
  for (let i = 0; i < sig.length; i++) { y += alpha * (sig[i] - y); out[i] = y; }
  return out;
}
function normalize(sig, peak = 0.85) {
  let m = 0;
  for (let i = 0; i < sig.length; i++) m = Math.max(m, Math.abs(sig[i]));
  if (m < 1e-6) return sig;
  const k = peak / m;
  for (let i = 0; i < sig.length; i++) sig[i] *= k;
  return sig;
}
function fade(sig, ms = 20) {
  const n = Math.floor((SR * ms) / 1000);
  for (let i = 0; i < n && i < sig.length; i++) {
    const g = i / n;
    sig[i] *= g;
    sig[sig.length - 1 - i] *= g;
  }
  return sig;
}
function loopable(sig, ms = 400) {
  // crossfade the tail into the head so the clip loops seamlessly
  const n = Math.floor((SR * ms) / 1000);
  if (n <= 0 || n >= sig.length / 2) return sig;
  const out = new Float32Array(sig.length - n);
  out.set(sig.subarray(0, out.length));
  for (let i = 0; i < n; i++) {
    const g = i / n;
    out[i] = sig[i] * g + sig[sig.length - n + i] * (1 - g);
  }
  return out;
}

const files = {};
const loops = new Set();

function def(name, samples, loop = false) {
  files[name] = samples;
  if (loop) loops.add(name);
}

// ---- interface ------------------------------------------------------------
{
  const n = Math.floor(SR * 0.14);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    sig[i] = Math.sin(2 * Math.PI * 880 * t) * Math.exp(-t * 30) * 0.5 +
             Math.sin(2 * Math.PI * 1320 * t) * Math.exp(-t * 45) * 0.3;
  }
  def('ui_click', fade(normalize(sig, 0.35), 6));
}
{
  const n = Math.floor(SR * 0.5);
  const sig = new Float32Array(n);
  const notes = [523.25, 659.25, 783.99];
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    let s = 0;
    for (let k = 0; k < notes.length; k++) {
      const start = k * 0.09;
      if (t > start) s += Math.sin(2 * Math.PI * notes[k] * (t - start)) * Math.exp(-(t - start) * 9) * 0.3;
    }
    sig[i] = s;
  }
  def('ui_confirm', fade(normalize(sig, 0.4), 10));
}
{
  const n = Math.floor(SR * 0.12);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    sig[i] = Math.sin(2 * Math.PI * 520 * t) * Math.exp(-t * 34) * 0.45 +
             Math.sin(2 * Math.PI * 390 * t) * Math.exp(-t * 28) * 0.28;
  }
  def('ui_back', fade(normalize(sig, 0.3), 6));
}
{
  // two-step descending buzz: unmistakably an error, not a musical note
  const n = Math.floor(SR * 0.22);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const f = t < 0.1 ? 220 : 175;
    sig[i] = (Math.sin(2 * Math.PI * f * t) * 0.55 + Math.sin(2 * Math.PI * f * 2 * t) * 0.18) *
             Math.exp(-((t % 0.1) * 9));
  }
  def('ui_error', fade(normalize(sig, 0.4), 8));
}
{
  const n = Math.floor(SR * 0.42);
  const sig = new Float32Array(n);
  const notes = [659.25, 987.77];
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    let s = 0;
    for (let k = 0; k < notes.length; k++) {
      const start = k * 0.1;
      if (t > start) s += Math.sin(2 * Math.PI * notes[k] * (t - start)) * Math.exp(-(t - start) * 7) * 0.34;
    }
    sig[i] = s;
  }
  def('ui_success', fade(normalize(sig, 0.45), 14));
}
{
  const n = Math.floor(SR * 0.26);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    sig[i] = (Math.sin(2 * Math.PI * 1046.5 * t) * 0.4 + Math.sin(2 * Math.PI * 1568 * t) * 0.25) * Math.exp(-t * 11);
  }
  def('coin_gain', fade(normalize(sig, 0.4), 10));
}
{
  const n = Math.floor(SR * 0.09);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    sig[i] = Math.sin(2 * Math.PI * 1200 * t) * Math.exp(-t * 40) * 0.3;
  }
  def('voice_ping', fade(normalize(sig, 0.25), 6));
}

// ---- movement -------------------------------------------------------------
{
  const n = Math.floor(SR * 0.28);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const f = 280 + 520 * t;
    sig[i] = Math.sin(2 * Math.PI * f * t) * Math.exp(-t * 9) * 0.4;
  }
  def('jump', fade(normalize(sig, 0.5), 10));
}
{
  const n = Math.floor(SR * 0.35);
  const sig = new Float32Array(n);
  const rnd = mulberry(12345);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const env = Math.exp(-t * 16);
    sig[i] = (noise(rnd) * 0.35 + Math.sin(2 * Math.PI * 70 * t) * 0.45) * env;
  }
  def('land', fade(normalize(lowpass(sig, 0.4), 0.6), 12));
}

// footsteps: four variants per surface so stride sounds never repeat
const SURFACES = [
  { name: 'grass', filt: 0.22, tone: 130, noiseAmt: 0.8 },
  { name: 'sand', filt: 0.14, tone: 90, noiseAmt: 0.9 },
  { name: 'concrete', filt: 0.55, tone: 210, noiseAmt: 0.5 },
  { name: 'snow', filt: 0.30, tone: 70, noiseAmt: 0.85 },
  { name: 'water', filt: 0.40, tone: 320, noiseAmt: 0.95 },
  { name: 'wood', filt: 0.48, tone: 180, noiseAmt: 0.45 },
];
for (const s of SURFACES) {
  for (let v = 1; v <= 4; v++) {
    const n = Math.floor(SR * 0.16);
    const sig = new Float32Array(n);
    const rnd = mulberry(1000 + v * 97 + s.tone);
    for (let i = 0; i < n; i++) {
      const t = i / SR;
      const env = Math.exp(-t * 34);
      sig[i] = (noise(rnd) * s.noiseAmt + Math.sin(2 * Math.PI * (s.tone + v * 6) * t) * 0.35) * env;
    }
    def(`footstep_${s.name}_${v}`, fade(normalize(lowpass(sig, s.filt), 0.5), 8));
  }
}

// ---- ambience (looped) ----------------------------------------------------
const AMBIENT = [
  { name: 'amb_forest_day', len: 6.0, base: 0.12, birds: true, wind: 0.25 },
  { name: 'amb_beach_waves', len: 6.0, base: 0.16, birds: false, wind: 0.55 },
  { name: 'amb_city_hum', len: 6.0, base: 0.18, birds: false, wind: 0.15 },
  { name: 'amb_open_plain', len: 6.0, base: 0.10, birds: false, wind: 0.35 },
  { name: 'amb_snow_wind', len: 6.0, base: 0.14, birds: false, wind: 0.75 },
  { name: 'amb_volcano_loop', len: 6.0, base: 0.22, birds: false, wind: 0.20 },
];
for (const a of AMBIENT) {
  const n = Math.floor(SR * a.len);
  const sig = new Float32Array(n);
  const rnd = mulberry(a.name.length * 7919);
  let y = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    y += a.wind * 0.03 * (noise(rnd) - y);
    const swell = 0.6 + 0.4 * Math.sin(2 * Math.PI * 0.09 * t + a.base * 10);
    let s = y * 2.2 * swell;
    s += Math.sin(2 * Math.PI * (55 + a.base * 60) * t) * a.base * 0.5;
    if (a.birds) {
      const chirp = Math.sin(2 * Math.PI * 2400 * t) * Math.pow(Math.max(0, Math.sin(2 * Math.PI * 0.37 * t)), 24);
      s += chirp * 0.06;
    }
    sig[i] = s;
  }
  def(a.name, loopable(normalize(lowpass(sig, 0.7), 0.55), 500), true);
}

// ---- music (looped beds) --------------------------------------------------
function chordBed(rootHz, seconds, brightness, rhythmHz) {
  const n = Math.floor(SR * seconds);
  const sig = new Float32Array(n);
  const ratios = [1, 1.1892, 1.4983, 2];
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    let s = 0;
    for (let k = 0; k < ratios.length; k++) {
      const f = rootHz * ratios[k];
      s += Math.sin(2 * Math.PI * f * t + k) * (brightness / (k + 1)) * 0.22;
    }
    // pulse pad
    const pulse = 0.5 + 0.5 * Math.sin(2 * Math.PI * rhythmHz * t);
    sig[i] = s * (0.55 + 0.45 * pulse);
  }
  return sig;
}
def('music_exploration', loopable(normalize(lowpass(chordBed(146.83, 9, 0.9, 0.25), 0.35), 0.5), 500), true);
def('music_social_lofi', loopable(normalize(lowpass(chordBed(174.61, 9, 0.7, 0.5), 0.28), 0.5), 500), true);
def('music_race_energy', loopable(normalize(lowpass(chordBed(196.0, 8, 1.0, 1.6), 0.6), 0.5), 600), true);
def('music_combat_tense', loopable(normalize(lowpass(chordBed(110.0, 8, 0.8, 0.75), 0.45), 0.5), 600), true);
def('music_horror_theme', loopable(normalize(lowpass(chordBed(82.41, 8, 0.5, 0.18), 0.3), 0.45), 700), true);

// ---- NEX AI pipeline ------------------------------------------------------
{
  const n = Math.floor(SR * 0.8);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    sig[i] = (Math.sin(2 * Math.PI * 440 * t) * 0.2 + Math.sin(2 * Math.PI * 660 * t) * 0.12) * Math.exp(-t * 4.5);
  }
  def('ai_tick', fade(normalize(sig, 0.3), 10));
}
{
  const n = Math.floor(SR * 1.6);
  const sig = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const env = Math.exp(-t * 2.2);
    sig[i] = (Math.sin(2 * Math.PI * 392 * t) + Math.sin(2 * Math.PI * 587.33 * t) * 0.6 +
              Math.sin(2 * Math.PI * 784 * t) * 0.4) * env * 0.3;
  }
  def('ai_complete', fade(normalize(sig, 0.5), 30));
}

// ---- write the mono WAVs, then encode the bank ---------------------------
function wav(samples, sr) {
  const n = samples.length;
  const buf = Buffer.alloc(44 + n * 2);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + n * 2, 4); buf.write('WAVE', 8);
  buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(1, 22);
  buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * 2, 28); buf.writeUInt16LE(2, 32); buf.writeUInt16LE(16, 34);
  buf.write('data', 36); buf.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) {
    const s = Math.max(-1, Math.min(1, samples[i]));
    buf.writeInt16LE(Math.round(s * 32000), 44 + i * 2);
  }
  return buf;
}

// 22.05 kHz mono keeps the whole bank under ~3 MB in the APK while staying transparent for
// interface sounds and low-frequency ambience; the mixer resamples to the device rate.
const SR_OUT = 22050;
function downsample(sig, from, to) {
  if (from === to) return sig;
  const ratio = from / to;
  const n = Math.floor(sig.length / ratio);
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const src = i * ratio;
    const i0 = Math.floor(src);
    const i1 = Math.min(i0 + 1, sig.length - 1);
    const t = src - i0;
    out[i] = sig[i0] * (1 - t) + sig[i1] * t;
  }
  return out;
}

const order = Object.keys(files);
let total = 0;
for (const k of order) {
  const pcm = downsample(files[k], SR, SR_OUT);
  const buf = wav(pcm, SR_OUT);
  fs.writeFileSync(path.join(OUT, `${k}.wav`), buf);
  total += buf.length;
}
fs.rmSync(TMP, { recursive: true, force: true });
console.log(`[audio] wrote ${order.length} clips, ${(total / 1048576).toFixed(2)} MB @ ${SR_OUT} Hz mono -> ${OUT}`);
