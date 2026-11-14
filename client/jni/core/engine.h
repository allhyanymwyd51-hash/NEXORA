// NEXORA engine — platform lifecycle, render loop, input and global services.
#pragma once
#include "font.h"
#include "gfx.h"
#include "net.h"
#include "sys.h"

namespace nx {

struct Touch {
  int id = 0;
  Vec2 pos, start;
  double time = 0, startTime = 0;
  bool down = false, moved = false;
};

enum class Quality { Low, Medium, High, Ultra, Auto };

struct QualitySettings {
  Quality tier = Quality::Auto;
  int shadowMapSize = 1024;
  bool dynamicShadows = true;
  float viewDistance = 260.0f;
  float vegetationDensity = 1.0f;
  float resolutionScale = 1.0f;
  int particleBudget = 400;
  bool waterReflection = false;
  bool bloom = true;
  int terrainDetail = 1;   // 0 low, 1 mid, 2 high
  bool antialias = false;
};

class Engine {
 public:
  static Engine& get();

  bool init(JNIEnv* env, JavaVM* vm);
  void startRenderThread();
  void stopRenderThread();

  // surface lifecycle (called from Java)
  void onSurfaceCreated(jobject surface);
  void onSurfaceChanged(jobject surface, int w, int h);
  void onSurfaceDestroyed();
  void onPause();
  void onResume();

  // per-frame app hook implemented in app.cpp
  void frame(double dt);

  float width() const { return m_width; }
  float height() const { return m_height; }
  float density() const { return m_density; }
  float safeTop() const { return m_safeTop; }
  float safeBottom() const { return m_safeBottom; }
  float safeLeft() const { return m_safeLeft; }
  float safeRight() const { return m_safeRight; }
  bool landscape() const { return m_width > m_height; }

  double time() const { return m_time; }

  // input
  void pushTouchDown(int id, float x, float y, double timeMs);
  void pushTouchMove(int id, float x, float y, double timeMs);
  void pushTouchUp(int id, float x, float y, double timeMs);
  void pushText(const char* utf8);
  void pushKey(int keyCode);
  Touch* findTouch(int id);
  int touchCount() const { return m_touchCount; }
  Touch* touches() { return m_touches; }
  bool backPressed();   // consumes
  void consumeKeys(char* out, size_t cap);

  // services
  Font& font(int weight);            // 0 regular, 1 semibold, 2 bold
  SpriteBatch& sprites() { return m_sprites; }
  // 2D layer used by the UI (glyphs + icons); kept separate so world sprites never interleave
  SpriteBatch& uiSprites() { return m_uiSprites; }
  TextureBank& tex() { return textures; }
  QualitySettings& quality() { return m_quality; }
  void applyQuality(Quality q);
  void detectDeviceTier();
  void requestMicPermission();
  bool micGranted() const { return m_micGranted; }
  void setMicGranted(bool g) { m_micGranted = g; }

  // audio (mixer implemented in audio.cpp)
  void audioInit();
  void audioShutdown();
  void setMasterVolume(float v);
  void setMusicVolume(float v);
  void playSfx(const char* name, float gain = 1.0f);
  void playMusic(const char* name);
  void stopMusic();
  void setVoiceMicEnabled(bool on);
  bool voiceMicEnabled() const;
  void pushVoiceFrame(const short* samples, int count);  // from mic callback
  int popVoiceFrame(short* out, int maxSamples);          // for network sender

  jobject surfaceObject() { return m_surface; }
  int fps() const { return m_fps; }
  float frameMs() const { return m_frameMs; }
  void toast(const char* msg);
  // soft keyboard (the activity owns the IME; the engine only asks)
  void showKeyboard();
  void hideKeyboard();

 private:
  Engine() {}
  void renderLoop();
  bool createEgl();
  void destroyEgl();
  void handleInput(double dt);

  EGLDisplay m_display = nullptr;
  EGLSurface m_eglSurface = nullptr;
  EGLContext m_context = nullptr;
  EGLConfig m_config = nullptr;
  jobject m_surface = nullptr;
  ANativeWindow* m_window = nullptr;
  int m_width = 0, m_height = 0;
  float m_density = 1.0f;
  float m_safeTop = 0, m_safeBottom = 0, m_safeLeft = 0, m_safeRight = 0;
  double m_time = 0;
  int m_fps = 60;
  float m_frameMs = 16.7f;
  QualitySettings m_quality;
  Font m_fontRegular, m_fontSemi, m_fontBold;
  SpriteBatch m_sprites;
  SpriteBatch m_uiSprites;
  Touch m_touches[10];
  int m_touchCount = 0;
  bool m_backPressed = false;
  char m_keyQueue[256] = {0};
  size_t m_keyLen = 0;
  bool m_micGranted = false;
  volatile bool m_running = false;
  volatile bool m_paused = false;
  unsigned long m_thread = 0;
};

// JNI bridge implementations that need app access
void onPermissionResult(const char* permission, bool granted);

}  // namespace nx
