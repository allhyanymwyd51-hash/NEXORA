#include "audio.h"
#include "engine.h"

namespace nx {

static const int kMaxVoices = 48;
static const int kMaxSounds = 96;
static const int kMaxMusic = 4;

struct VoiceBus {
  SoundVoice voices[kMaxVoices];
  Sound* sounds[kMaxSounds];
  int soundCount = 0;

  AAudioStream* outStream = nullptr;
  AAudioStream* micStream = nullptr;
  int sampleRate = 48000;
  int framesPerBuffer = 256;
  float masterGain = 0.85f;
  float musicGain = 0.55f;
  volatile bool micEnabled = false;

  Vec3 listener{0, 1.6f, 0};
  Vec3 listenerForward{0, 0, -1};

  // mic ring buffer for the network sender
  static const int kMicCap = 48000 * 2;  // 1 second stereo
  short* micRing = nullptr;
  volatile int micRead = 0, micWrite = 0;
  char micRingMutex = 0;

  // remote peers
  struct Peer { int id; short* buf; int cap; volatile int read; volatile int write; };
  Peer peers[8];

  Buf fileData;  // keeps WAV bytes alive
};

static VoiceBus* g = nullptr;

// ---------------------------------------------------------------------------
// WAV parsing (16-bit PCM, mono or stereo)
// ---------------------------------------------------------------------------
static uint32_t rdU32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rdU16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

Sound* audioLoadWav(const char* assetPath, const char* alias, bool loop) {
  if (!g) return nullptr;
  if (g->soundCount >= kMaxSounds) return nullptr;
  void* data = nullptr;
  size_t size = 0;
  if (!readAsset(assetPath, &data, &size) || size < 44) {
    if (data) freeAsset(data);
    return nullptr;
  }
  const unsigned char* p = (const unsigned char*)data;
  if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) { freeAsset(data); return nullptr; }
  size_t pos = 12;
  int channels = 2, bits = 16, sr = 44100;
  const unsigned char* pcm = nullptr;
  uint32_t pcmLen = 0;
  while (pos + 8 <= size) {
    const unsigned char* chunk = p + pos;
    uint32_t clen = rdU32(chunk + 4);
    if (memcmp(chunk, "fmt ", 4) == 0 && clen >= 16) {
      uint16_t audioFormat = rdU16(chunk + 8);
      channels = rdU16(chunk + 10);
      sr = (int)rdU32(chunk + 12);
      bits = rdU16(chunk + 22);
      if (audioFormat != 1) { freeAsset(data); NX_LOGW("unsupported wav format %s", alias); return nullptr; }
    } else if (memcmp(chunk, "data", 4) == 0) {
      pcm = chunk + 8;
      pcmLen = clen;
      break;
    }
    pos += 8 + clen + (clen & 1);
  }
  if (!pcm || pcmLen == 0 || bits != 16) { freeAsset(data); return nullptr; }
  int inFrames = (int)(pcmLen / (2 * channels));
  // resample to the device rate (linear interpolation — WAV bank is 44.1k, devices are usually 48k)
  double ratio = (double)sr / (double)g->sampleRate;
  int outFrames = (int)(inFrames / ratio) + 1;
  Sound* s = new Sound();
  strncpy(s->name, alias, sizeof s->name - 1);
  s->frames = outFrames;
  s->channels = 2;
  s->sampleRate = g->sampleRate;
  s->loop = loop;
  s->duration = (float)outFrames / (float)g->sampleRate;
  s->samples = (short*)malloc((size_t)outFrames * 4);
  const short* in = (const short*)pcm;
  for (int i = 0; i < outFrames; i++) {
    double srcPos = i * ratio;
    int i0 = (int)srcPos;
    int i1 = i0 + 1 < inFrames ? i0 + 1 : i0;
    float t = (float)(srcPos - i0);
    short l0 = in[i0 * channels];
    short l1 = in[i1 * channels];
    short r0 = channels > 1 ? in[i0 * channels + 1] : l0;
    short r1 = channels > 1 ? in[i1 * channels + 1] : l1;
    s->samples[i * 2] = (short)(l0 + (l1 - l0) * t);
    s->samples[i * 2 + 1] = (short)(r0 + (r1 - r0) * t);
  }
  freeAsset(data);
  // seamless loop: crossfade the last 40ms into the first 40ms
  if (loop && s->frames > 4096) {
    int fade = g->sampleRate / 25;
    for (int i = 0; i < fade; i++) {
      float t = (float)i / fade;
      int a = s->frames - fade + i;
      s->samples[i * 2] = (short)(s->samples[i * 2] * t + s->samples[a * 2] * (1 - t));
      s->samples[i * 2 + 1] = (short)(s->samples[i * 2 + 1] * t + s->samples[a * 2 + 1] * (1 - t));
    }
    s->frames -= fade;
  }
  g->sounds[g->soundCount++] = s;
  return s;
}


// ---------------------------------------------------------------------------
// Streamed music: IMA-ADPCM (.adp). See tools/adp.py for the encoder; the block
// layout is duplicated in one place only so the two cannot drift silently.
// ---------------------------------------------------------------------------
static const int kAdpStep[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
    449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
    8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
    29794, 32767};
static const int kAdpIndex[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

static inline int adpNibble(int nib, int& pred, int& idx) {
  int step = kAdpStep[idx];
  int d = step >> 3;
  if (nib & 1) d += step >> 2;
  if (nib & 2) d += step >> 1;
  if (nib & 4) d += step;
  pred = (nib & 8) ? pred - d : pred + d;
  if (pred > 32767) pred = 32767;
  if (pred < -32768) pred = -32768;
  idx += kAdpIndex[nib];
  if (idx < 0) idx = 0;
  if (idx > 88) idx = 88;
  return pred;
}

// Decodes the next block into the rolling window (which holds lastBlock + current).
static bool adpNextBlock(Sound* s) {
  if (s->adpEof || !s->adp) return false;
  int chunk = s->adpChunk, ch = 2;
  int bytes = ch * 3 + ch * (chunk / 2);
  if (s->adpCursor + bytes > s->adpBytes) { s->adpEof = 1; return false; }
  const unsigned char* src = s->adp + s->adpCursor;
  s->adpCursor += bytes;
  // shift the previous block out, keep nothing (voices read [0,chunk) then we swap)
  short* dst = s->samples;
  for (int c = 0; c < ch; c++) {
    int pred = (short)(src[c * 3] | (src[c * 3 + 1] << 8));
    int idx = src[c * 3 + 2];
    if (idx > 88) idx = 88;
    const unsigned char* nib = src + ch * 3 + c * (chunk / 2);
    dst[c] = (short)pred;
    if (ch == 1) dst[1] = (short)pred;
    for (int f = 1; f < chunk; f++) {
      int n = f - 1;
      int byte = nib[n >> 1];
      int v = (n & 1) ? (byte & 0x0f) : (byte >> 4);
      int p = adpNibble(v, pred, idx);
      dst[f * 2 + c] = (short)p;
      if (ch == 1) dst[f * 2 + 1] = (short)p;
    }
  }
  s->frames = chunk;
  return true;
}

Sound* audioLoadAdpcm(const char* assetPath, const char* alias, bool loop) {
  if (!g) return nullptr;
  if (g->soundCount >= kMaxSounds) return nullptr;
  void* data = nullptr;
  size_t size = 0;
  if (!readAsset(assetPath, &data, &size) || !data || size < 18) return nullptr;
  const unsigned char* p = (const unsigned char*)data;
  if (memcmp(p, "NXAD", 4) != 0) { NX_LOGW("not adpcm: %s", alias); return nullptr; }
  int ch = p[6] | (p[7] << 8);
  int chunk = p[16] | (p[17] << 8);
  int total = (int)(p[12] | (p[13] << 8) | (p[14] << 16) | ((uint32_t)p[15] << 24));
  if (ch != 2 || chunk < 2 || chunk > 4096) { NX_LOGW("bad adpcm header: %s", alias); return nullptr; }
  Sound* s = new Sound();
  strncpy(s->name, alias, sizeof s->name - 1);
  s->adp = (unsigned char*)data;
  s->adpBytes = (int)size;
  s->adpCursor = 18;
  s->adpChunk = chunk;
  s->adpTotalFrames = total;
  s->sampleRate = g->sampleRate;
  s->loop = loop;
  s->duration = (float)total / 44100.0f;
  s->samples = (short*)malloc((size_t)chunk * 2 * sizeof(short));
  if (!s->samples) { free(data); delete s; return nullptr; }
  adpNextBlock(s);
  g->sounds[g->soundCount++] = s;
  NX_LOGI("music stream: %s %d frames (%.1fs, %.1f MB)", alias, total, s->duration, size / 1048576.0f);
  return s;
}

// ---------------------------------------------------------------------------
// Bank: every clip the game can play, with its loop flag. Kept as a table so a
// missing asset is a single warning line instead of a silent dead button.
// ---------------------------------------------------------------------------
struct BankEntry {
  const char* name;
  bool loop;
};
static const BankEntry kBank[] = {
    {"ui_click", false},      {"ui_confirm", false},    {"ui_back", false},
    {"ui_error", false},      {"ui_success", false},    {"ui_hover", false},
    {"coin_gain", false},     {"voice_ping", false},
    {"jump", false},          {"land", false},
    {"footstep_grass_1", false}, {"footstep_grass_2", false}, {"footstep_grass_3", false}, {"footstep_grass_4", false},
    {"footstep_sand_1", false}, {"footstep_sand_2", false}, {"footstep_sand_3", false}, {"footstep_sand_4", false},
    {"footstep_concrete_1", false}, {"footstep_concrete_2", false}, {"footstep_concrete_3", false}, {"footstep_concrete_4", false},
    {"footstep_snow_1", false}, {"footstep_snow_2", false}, {"footstep_snow_3", false}, {"footstep_snow_4", false},
    {"footstep_water_1", false}, {"footstep_water_2", false}, {"footstep_water_3", false}, {"footstep_water_4", false},
    {"footstep_wood_1", false}, {"footstep_wood_2", false}, {"footstep_wood_3", false}, {"footstep_wood_4", false},
    {"amb_forest_day", true}, {"amb_beach_waves", true}, {"amb_city_hum", true}, {"amb_open_plain", true},
    {"amb_snow_wind", true}, {"amb_volcano_loop", true},
    {"music_exploration", true}, {"music_social_lofi", true}, {"music_race_energy", true},
    {"music_combat_tense", true}, {"music_horror_theme", true}, {"music_menu", true},
    {"music_victory", true}, {"music_city", true}, {"music_storm", true},
    {"ai_tick", false}, {"ai_complete", false},
};

int audioLoadBank() {
  if (!g) return 0;
  int loaded = 0;
  for (const BankEntry& e : kBank) {
    if (audioFind(e.name)) continue;
    char path[128];
    // Music ships as compressed streams (audio/music/<name>.adp); everything else is WAV.
    if (strncmp(e.name, "music_", 6) == 0) {
      snprintf(path, sizeof path, "audio/music/%s.adp", e.name + 6);
      if (audioLoadAdpcm(path, e.name, e.loop)) { loaded++; continue; }
      snprintf(path, sizeof path, "audio/%s.wav", e.name);
    } else {
      snprintf(path, sizeof path, "audio/%s.wav", e.name);
    }
    if (audioLoadWav(path, e.name, e.loop)) loaded++;
    else NX_LOGW("sound missing: %s", path);
  }
  NX_LOGI("audio bank: %d/%d clips", g->soundCount, (int)(sizeof(kBank) / sizeof(kBank[0])));
  return loaded;
}

Sound* audioFind(const char* name) {
  if (!g) return nullptr;
  for (int i = 0; i < g->soundCount; i++) {
    if (strcmp(g->sounds[i]->name, name) == 0) return g->sounds[i];
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// mixing
// ---------------------------------------------------------------------------
static void mixInto(short* out, int frames) {
  static float accL[4096], accR[4096];
  int n = frames > 4096 ? 4096 : frames;
  for (int i = 0; i < n; i++) { accL[i] = 0; accR[i] = 0; }
  for (int v = 0; v < kMaxVoices; v++) {
    SoundVoice& voice = g->voices[v];
    if (!voice.active || !voice.sound) continue;
    Sound* s = voice.sound;
    for (int i = 0; i < n; i++) {
      if (voice.cursor >= s->frames) {
        if (voice.loop) voice.cursor = 0;
        else { voice.active = false; break; }
      }
      accL[i] += s->samples[voice.cursor * 2] * (1.0f / 32768.0f) * voice.gainL;
      accR[i] += s->samples[voice.cursor * 2 + 1] * (1.0f / 32768.0f) * voice.gainR;
      voice.cursor++;
    }
  }
  // music bus
  for (int m = 0; m < kMaxMusic; m++) {
    SoundVoice& voice = g->voices[kMaxVoices - kMaxMusic + m];
    if (!voice.active || !voice.sound) continue;
    Sound* s = voice.sound;
    float gain = voice.gainL * g->musicGain;
    for (int i = 0; i < n; i++) {
      if (voice.cursor >= s->frames) {
        if (s->adp) {
          // Streamed track: pull the next block; wrap to the start when it ends.
          if (!adpNextBlock(s)) {
            if (!voice.loop) { voice.active = false; break; }
            s->adpCursor = 18;
            s->adpEof = 0;
            if (!adpNextBlock(s)) { voice.active = false; break; }
          }
          voice.cursor = 0;
        } else if (voice.loop) voice.cursor = 0;
        else { voice.active = false; break; }
      }
      accL[i] += s->samples[voice.cursor * 2] * (1.0f / 32768.0f) * gain;
      accR[i] += s->samples[voice.cursor * 2 + 1] * (1.0f / 32768.0f) * gain;
      voice.cursor++;
    }
  }
  // voice peers (positional attenuation applied by the caller when pushing frames)
  for (int p = 0; p < 8; p++) {
    VoiceBus::Peer& peer = g->peers[p];
    if (peer.id == 0 || !peer.buf) continue;
    int available = (peer.write - peer.read + peer.cap) % peer.cap;
    for (int i = 0; i < n && i * 2 + 1 < available; i++) {
      if (peer.read == peer.write) break;
      float s0 = peer.buf[peer.read] / 32768.0f;
      peer.read = (peer.read + 1) % peer.cap;
      float s1 = peer.buf[peer.read] / 32768.0f;
      peer.read = (peer.read + 1) % peer.cap;
      accL[i] += s0 * g->masterGain;
      accR[i] += s1 * g->masterGain;
    }
  }
  for (int i = 0; i < n; i++) {
    float l = clampf(accL[i] * g->masterGain, -1.0f, 1.0f);
    float r = clampf(accR[i] * g->masterGain, -1.0f, 1.0f);
    // soft clip
    l = l - (l * l * l) * 0.15f;
    r = r - (r * r * r) * 0.15f;
    out[i * 2] = (short)(l * 32000.0f);
    out[i * 2 + 1] = (short)(r * 32000.0f);
  }
}

static int32_t outCallback(AAudioStream*, void* userData, void* audioData, int32_t numFrames) {
  (void)userData;
  mixInto((short*)audioData, numFrames);
  return 0;  // AAUDIO_CALLBACK_RESULT_CONTINUE
}

static void inCallback(AAudioStream*, void* userData, void* audioData, int32_t numFrames) {
  (void)userData;
  if (!g || !g->micEnabled || !g->micRing) return;
  const short* in = (const short*)audioData;
  for (int i = 0; i < numFrames; i++) {
    int next = (g->micWrite + 2) % g->kMicCap;
    if (next == g->micRead) break;  // overflow: drop
    g->micRing[g->micWrite] = in[i];
    g->micRing[(g->micWrite + 1) % g->kMicCap] = in[i];
    g->micWrite = next;
  }
}

void audioInit() {
  if (g) return;
  g = new VoiceBus();
  g->micRing = (short*)calloc(g->kMicCap, sizeof(short));
  for (int i = 0; i < 8; i++) {
    g->peers[i].cap = 48000 * 2;
    g->peers[i].buf = (short*)calloc(g->peers[i].cap, sizeof(short));
  }
  // output stream
  AAudioStreamBuilder* b = nullptr;
  if (AAudio_createStreamBuilder(&b) == AAUDIO_OK && b) {
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setSampleRate(b, 48000);
    AAudioStreamBuilder_setDataCallback(b, (aaudio_data_callback_t)outCallback, nullptr);
    if (AAudioStreamBuilder_openStream(b, &g->outStream) == AAUDIO_OK && g->outStream) {
      g->sampleRate = AAudioStream_getSampleRate(g->outStream);
      g->framesPerBuffer = AAudioStream_getFramesPerBurst(g->outStream) * 2;
      AAudioStream_setBufferSizeInFrames(g->outStream, g->framesPerBuffer);
      AAudioStream_requestStart(g->outStream);
      NX_LOGI("audio output %d Hz, %d frames", g->sampleRate, g->framesPerBuffer);
    }
    AAudioStreamBuilder_delete(b);
  }
  if (!g->outStream) NX_LOGW("audio output unavailable");
}

void audioShutdown() {
  if (!g) return;
  audioMicStop();
  if (g->outStream) {
    AAudioStream_requestStop(g->outStream);
    AAudioStream_close(g->outStream);
  }
  for (int i = 0; i < g->soundCount; i++) {
    free(g->sounds[i]->samples);
    delete g->sounds[i];
  }
  for (int i = 0; i < 8; i++) free(g->peers[i].buf);
  free(g->micRing);
  delete g;
  g = nullptr;
}

void audioSetSampleRate(int rate) { if (g) g->sampleRate = rate; }
void audioSetFramesPerBuffer(int frames) { if (g) g->framesPerBuffer = frames; }
int audioSampleRate() { return g ? g->sampleRate : 48000; }

static SoundVoice* allocVoice(bool musicBus) {
  int start = musicBus ? kMaxVoices - kMaxMusic : 0;
  int end = musicBus ? kMaxVoices : kMaxVoices - kMaxMusic;
  for (int i = start; i < end; i++) {
    if (!g->voices[i].active) return &g->voices[i];
  }
  // steal the quietest
  SoundVoice* best = &g->voices[start];
  for (int i = start; i < end; i++) {
    if (g->voices[i].gainL < best->gainL) best = &g->voices[i];
  }
  return best;
}

void audioPlay(const char* name, float gain) {
  if (!g) return;
  Sound* s = audioFind(name);
  if (!s) return;
  SoundVoice* v = allocVoice(false);
  v->sound = s;
  v->cursor = 0;
  v->gainL = gain;
  v->gainR = gain;
  v->active = true;
  v->loop = false;
  v->spatial = false;
}

int audioPlaySpatial(const char* name, const Vec3& pos, const Vec3& listener, float gain, float maxDistance) {
  if (!g) return -1;
  Sound* s = audioFind(name);
  if (!s) return -1;
  float dist = (pos - listener).length();
  if (dist > maxDistance) return -1;
  float att = 1.0f - clampf(dist / maxDistance, 0, 1);
  att *= att;
  // simple stereo pan based on the listener's right vector
  Vec3 right = cross(g->listenerForward, Vec3{0, 1, 0}).normalized();
  Vec3 to = (pos - listener).normalized();
  float pan = clampf(dot(to, right), -1, 1) * 0.7f;
  int idx = -1;
  for (int i = 0; i < kMaxVoices - kMaxMusic; i++) if (!g->voices[i].active) { idx = i; break; }
  if (idx < 0) return -1;
  SoundVoice& v = g->voices[idx];
  v.sound = s;
  v.cursor = 0;
  v.gainL = gain * att * (1.0f - fmaxf(0, pan));
  v.gainR = gain * att * (1.0f + fminf(0, pan));
  v.active = true;
  v.loop = false;
  v.spatial = true;
  v.pos = pos;
  return idx;
}

void audioSetVoicePos(int handle, const Vec3& pos) {
  if (!g || handle < 0 || handle >= kMaxVoices) return;
  g->voices[handle].pos = pos;
}

void audioUpdateListener(const Vec3& pos, const Vec3& forward) {
  if (!g) return;
  g->listener = pos;
  g->listenerForward = forward;
}

void audioMusic(const char* name, float gain) {
  if (!g) return;
  Sound* s = audioFind(name);
  if (!s) return;
  for (int i = kMaxVoices - kMaxMusic; i < kMaxVoices; i++) {
    if (g->voices[i].active && g->voices[i].sound == s) return;  // already playing
  }
  for (int i = kMaxVoices - kMaxMusic; i < kMaxVoices; i++) g->voices[i].active = false;
  SoundVoice& v = g->voices[kMaxVoices - kMaxMusic];
  v.sound = s;
  v.cursor = 0;
  v.gainL = gain;
  v.gainR = gain;
  v.active = true;
  v.loop = true;
}

void audioStopMusic() {
  if (!g) return;
  for (int i = kMaxVoices - kMaxMusic; i < kMaxVoices; i++) g->voices[i].active = false;
}

void audioSetMasterGain(float v) { if (g) g->masterGain = clampf(v, 0, 1); }
void audioSetMusicGain(float v) { if (g) g->musicGain = clampf(v, 0, 1); }

void audioPushVoicePeer(int peerId, const short* samples, int count) {
  if (!g || peerId <= 0) return;
  VoiceBus::Peer* peer = nullptr;
  for (int i = 0; i < 8; i++) {
    if (g->peers[i].id == peerId) { peer = &g->peers[i]; break; }
    if (g->peers[i].id == 0) peer = &g->peers[i];
  }
  if (!peer) return;
  peer->id = peerId;
  for (int i = 0; i < count; i++) {
    int next = (peer->write + 1) % peer->cap;
    if (next == peer->read) break;
    peer->buf[peer->write] = samples[i];
    peer->write = next;
  }
}

bool audioMicStart() {
  if (!g) return false;
  if (g->micStream) { g->micEnabled = true; return true; }
  AAudioStreamBuilder* b = nullptr;
  if (AAudio_createStreamBuilder(&b) != AAUDIO_OK || !b) return false;
  AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_INPUT);
  AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setChannelCount(b, 1);
  AAudioStreamBuilder_setSampleRate(b, 16000);  // wideband voice
  AAudioStreamBuilder_setDataCallback(b, (aaudio_data_callback_t)inCallback, nullptr);
  aaudio_result_t rc = AAudioStreamBuilder_openStream(b, &g->micStream);
  AAudioStreamBuilder_delete(b);
  if (rc != AAUDIO_OK || !g->micStream) {
    g->micStream = nullptr;
    NX_LOGW("mic stream unavailable (%d)", (int)rc);
    return false;
  }
  AAudioStream_requestStart(g->micStream);
  g->micEnabled = true;
  g->micRead = g->micWrite = 0;
  NX_LOGI("mic started @ %d Hz", AAudioStream_getSampleRate(g->micStream));
  return true;
}

void audioMicStop() {
  if (!g) return;
  g->micEnabled = false;
  if (g->micStream) {
    AAudioStream_requestStop(g->micStream);
    AAudioStream_close(g->micStream);
    g->micStream = nullptr;
  }
}

}  // namespace nx

// engine hooks
namespace nx {
void Engine::audioInit() { nx::audioInit(); }
void Engine::audioShutdown() { nx::audioShutdown(); }
void Engine::setMasterVolume(float v) { audioSetMasterGain(v); }
void Engine::setMusicVolume(float v) { audioSetMusicGain(v); }
void Engine::playSfx(const char* name, float gain) { audioPlay(name, gain); }
void Engine::playMusic(const char* name) { audioMusic(name ? name : "", 0.55f); }
void Engine::stopMusic() { audioStopMusic(); }
void Engine::setVoiceMicEnabled(bool on) {
  if (on) audioMicStart();
  else audioMicStop();
}
bool Engine::voiceMicEnabled() const { return g && g->micEnabled; }
void Engine::pushVoiceFrame(const short* samples, int count) {
  if (!g || !g->micRing) return;
  for (int i = 0; i < count; i++) {
    int next = (g->micWrite + 2) % g->kMicCap;
    if (next == g->micRead) break;
    g->micRing[g->micWrite] = samples[i];
    g->micRing[(g->micWrite + 1) % g->kMicCap] = samples[i];
    g->micWrite = next;
  }
}
int Engine::popVoiceFrame(short* out, int maxSamples) {
  if (!g || !g->micRing) return 0;
  int n = 0;
  while (n < maxSamples && g->micRead != g->micWrite) {
    out[n++] = g->micRing[g->micRead];
    g->micRead = (g->micRead + 1) % g->kMicCap;
  }
  return n;
}
}  // namespace nx
