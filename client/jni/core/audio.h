// NEXORA engine — WAV bank mixer, positional SFX, music, mic capture + voice frames.
#pragma once
#include "sys.h"
#include "math.h"

namespace nx {

struct Sound {
  char name[48] = {0};
  short* samples = nullptr;   // interleaved stereo (rolling window for streamed tracks)
  int frames = 0;
  int channels = 2;
  int sampleRate = 44100;
  bool loop = false;
  float duration = 0;
  // Streamed IMA-ADPCM track: only a couple of seconds of PCM live in memory at a time,
  // so the 3-4 minute soundtrack costs RAM like a sound effect instead of like raw audio.
  unsigned char* adp = nullptr;   // owned compressed bytes
  int adpBytes = 0;
  int adpCursor = 18;             // next block offset
  int adpChunk = 0;               // frames per block
  int adpTotalFrames = 0;
  int adpEof = 0;
};

struct SoundVoice {
  Sound* sound = nullptr;
  int cursor = 0;
  float gainL = 0, gainR = 0;
  bool active = false;
  bool loop = false;
  bool spatial = false;
  Vec3 pos;
};

void audioInit();
void audioShutdown();
void audioSetSampleRate(int rate);
void audioSetFramesPerBuffer(int frames);
int audioSampleRate();

Sound* audioLoadWav(const char* assetPath, const char* alias, bool loop);
// Streamed music: IMA-ADPCM (.adp) decoded on demand, ~4:1 smaller than 16-bit PCM.
Sound* audioLoadAdpcm(const char* assetPath, const char* alias, bool loop);
// Loads the shipped sound bank (assets/audio/*.wav) into memory.
int audioLoadBank();
Sound* audioFind(const char* name);

void audioPlay(const char* name, float gain = 1.0f);
int audioPlaySpatial(const char* name, const Vec3& pos, const Vec3& listener, float gain = 1.0f,
                     float maxDistance = 60.0f);
void audioSetVoicePos(int handle, const Vec3& pos);
void audioUpdateListener(const Vec3& pos, const Vec3& forward);

void audioMusic(const char* name, float gain = 0.6f);
void audioStopMusic();
void audioSetMasterGain(float g);
void audioSetMusicGain(float g);

// remote voice peers (decoded PCM16 frames arriving from the network)
void audioPushVoicePeer(int peerId, const short* samples, int count);

bool audioMicStart();
void audioMicStop();

}  // namespace nx
