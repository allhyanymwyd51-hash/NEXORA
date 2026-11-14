#include "engine.h"
#include "app/app.h"

#ifndef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))
#endif
#ifndef JNICALL
#define JNICALL
#endif
#include "audio.h"
extern "C" jobject gActivity;

namespace nx {

// ---------------------------------------------------------------------------
// JNI entry points implemented by the engine
// ---------------------------------------------------------------------------
extern "C" JavaVM* gJavaVM;
extern void setAssetManagerGlobal(jobject am);
extern void setCacheDirPath(const char* p);
extern void setFilesDirPath(const char* p);

Engine& Engine::get() {
  static Engine engine;
  return engine;
}

static void setSafeAreaFromInsets(JNIEnv*, int) {}

bool Engine::init(JNIEnv* env, JavaVM* vm) {
  jni.env = env;
  jni.vm = vm;
  jni.attach();
  m_density = 2.0f;
  loadEGL();
  loadGL();
  return true;
}

Font& Engine::font(int weight) {
  if (weight >= 2) return m_fontBold;
  if (weight == 1) return m_fontSemi;
  return m_fontRegular;
}

void Engine::detectDeviceTier() {
  // Heuristic: screen resolution + reported GL renderer string.
  const char* renderer = (const char*)glGetString(GL_RENDERER);
  const char* version = (const char*)glGetString(GL_VERSION);
  NX_LOGI("GL_RENDERER=%s GL_VERSION=%s", renderer ? renderer : "?", version ? version : "?");
  int tier = 1;  // medium
  if (renderer) {
    if (strstr(renderer, "Adreno (TM) 7") || strstr(renderer, "Adreno (TM) 6") || strstr(renderer, "Mali-G7") ||
        strstr(renderer, "Mali-G8") || strstr(renderer, "Mali-G9") || strstr(renderer, "Xclipse") ||
        strstr(renderer, "Immortalis"))
      tier = 2;
    if (strstr(renderer, "Adreno (TM) 3") || strstr(renderer, "Adreno (TM) 4") || strstr(renderer, "Mali-4") ||
        strstr(renderer, "Mali-T") || strstr(renderer, "Mali-G3") || strstr(renderer, "Mali-G5"))
      tier = 0;
  }
  float pixels = m_width * m_height;
  if (pixels > 2.6e6f && tier == 2) tier = 3;  // ultra only for large high-end panels
  if (pixels < 1.2e6f && tier > 0) tier -= 1;
  Quality q = tier == 0 ? Quality::Low : tier == 1 ? Quality::Medium : tier == 2 ? Quality::High : Quality::Ultra;
  m_quality.tier = q;
  applyQuality(q);
}

void Engine::applyQuality(Quality q) {
  m_quality.tier = q;
  switch (q) {
    case Quality::Low:
      m_quality.shadowMapSize = 0;
      m_quality.dynamicShadows = false;
      m_quality.viewDistance = 150.0f;
      m_quality.vegetationDensity = 0.45f;
      m_quality.resolutionScale = 0.78f;
      m_quality.particleBudget = 120;
      m_quality.waterReflection = false;
      m_quality.bloom = false;
      m_quality.terrainDetail = 0;
      m_quality.antialias = false;
      break;
    case Quality::Medium:
      m_quality.shadowMapSize = 1024;
      m_quality.dynamicShadows = true;
      m_quality.viewDistance = 220.0f;
      m_quality.vegetationDensity = 0.8f;
      m_quality.resolutionScale = 0.9f;
      m_quality.particleBudget = 300;
      m_quality.waterReflection = false;
      m_quality.bloom = true;
      m_quality.terrainDetail = 1;
      m_quality.antialias = false;
      break;
    case Quality::High:
      m_quality.shadowMapSize = 2048;
      m_quality.dynamicShadows = true;
      m_quality.viewDistance = 320.0f;
      m_quality.vegetationDensity = 1.0f;
      m_quality.resolutionScale = 1.0f;
      m_quality.particleBudget = 500;
      m_quality.waterReflection = true;
      m_quality.bloom = true;
      m_quality.terrainDetail = 2;
      m_quality.antialias = false;
      break;
    case Quality::Ultra:
      m_quality.shadowMapSize = 2048;
      m_quality.dynamicShadows = true;
      m_quality.viewDistance = 420.0f;
      m_quality.vegetationDensity = 1.35f;
      m_quality.resolutionScale = 1.0f;
      m_quality.particleBudget = 800;
      m_quality.waterReflection = true;
      m_quality.bloom = true;
      m_quality.terrainDetail = 2;
      m_quality.antialias = true;
      break;
    case Quality::Auto:
      detectDeviceTier();
      return;
  }
  NX_LOGI("quality applied: tier=%d shadows=%d view=%.0f veg=%.2f", (int)q, m_quality.shadowMapSize,
          m_quality.viewDistance, m_quality.vegetationDensity);
}

// ---------------------------------------------------------------------------
// EGL
// ---------------------------------------------------------------------------
bool Engine::createEgl() {
  if (!m_display) {
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) return false;
    EGLint major = 0, minor = 0;
    if (!eglInitialize(m_display, &major, &minor)) return false;
    NX_LOGI("EGL %d.%d", major, minor);
  }
  if (!m_config) {
    const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
      EGL_NONE
    };
    EGLint n = 0;
    if (!eglChooseConfig(m_display, attribs, &m_config, 1, &n) || n < 1) {
      NX_LOGE("eglChooseConfig failed");
      return false;
    }
  }
  if (!m_surface || !m_window) return false;
  m_eglSurface = eglCreateWindowSurface(m_display, m_config, (EGLNativeWindowType)m_window, nullptr);
  if (m_eglSurface == EGL_NO_SURFACE) {
    NX_LOGE("eglCreateWindowSurface failed");
    return false;
  }
  if (!m_context) {
    const EGLint ctxAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, ctxAttribs);
    if (m_context == EGL_NO_CONTEXT) {
      NX_LOGE("eglCreateContext failed");
      return false;
    }
  }
  if (!eglMakeCurrent(m_display, m_eglSurface, m_eglSurface, m_context)) {
    NX_LOGE("eglMakeCurrent failed");
    return false;
  }
  eglSurfaceAttrib(m_display, m_eglSurface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
  EGLint w = 0, h = 0;
  eglQuerySurface(m_display, m_eglSurface, EGL_WIDTH, &w);
  eglQuerySurface(m_display, m_eglSurface, EGL_HEIGHT, &h);
  m_width = (float)w;
  m_height = (float)h;
  return true;
}

void Engine::destroyEgl() {
  if (m_display) {
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_eglSurface) eglDestroySurface(m_display, m_eglSurface);
    if (m_context) eglDestroyContext(m_display, m_context);
  }
  m_eglSurface = nullptr;
  m_context = nullptr;
  m_surface = nullptr;
  m_window = nullptr;
}

// ---------------------------------------------------------------------------
// Surface lifecycle
// ---------------------------------------------------------------------------
void Engine::onSurfaceCreated(jobject surface) {
  m_surface = surface;
  m_window = (ANativeWindow*)surface;
  if (!m_running) startRenderThread();
}

void Engine::onSurfaceChanged(jobject surface, int w, int h) {
  m_surface = surface;
  m_window = (ANativeWindow*)surface;
  m_width = (float)w;
  m_height = (float)h;
  m_safeTop = m_safeBottom = m_safeLeft = m_safeRight = 0;
  // Keep density in dp-ish units so UI scales identically across phones.
  float shortest = fminf((float)w, (float)h);
  m_density = clampf(shortest / 400.0f, 0.75f, 3.0f);
}

void Engine::onSurfaceDestroyed() {
  m_paused = true;
  if (m_context) {
    // GPU objects die with the surface; the app rebuilds them on resume.
    destroyEgl();
  }
  m_surface = nullptr;
  m_window = nullptr;
}

void Engine::onPause() { m_paused = true; }
void Engine::onResume() { m_paused = false; }

void Engine::startRenderThread() {
  if (m_running) return;
  m_running = true;
  pthread_create(&m_thread, nullptr, [](void* arg) -> void* {
    Engine::get().renderLoop();
    return nullptr;
  }, nullptr);
}

void Engine::stopRenderThread() { m_running = false; }

void Engine::renderLoop() {
  if (!createEgl()) {
    NX_LOGE("failed to create EGL context on render thread");
    return;
  }
  // GPU-side resources
  textures.build();
  m_sprites.init(0);
  m_uiSprites.init(0);
  // load fonts from the APK assets
  if (!m_fontRegular.loadFromAsset("fonts/Cairo-400.ttf")) NX_LOGE("regular font missing");
  if (!m_fontSemi.loadFromAsset("fonts/Cairo-600.ttf")) NX_LOGE("semibold font missing");
  if (!m_fontBold.loadFromAsset("fonts/Cairo-700.ttf")) NX_LOGE("bold font missing");
  audioInit();
  audioLoadBank();
  detectDeviceTier();
  app::init();

  double last = nowSeconds();
  double fpsAccum = 0;
  int fpsFrames = 0;
  while (m_running) {
    if (!m_eglSurface || m_paused) {
      usleep(16000);
      last = nowSeconds();
      continue;
    }
    if (!eglMakeCurrent(m_display, m_eglSurface, m_eglSurface, m_context)) {
      usleep(16000);
      continue;
    }
    double now = nowSeconds();
    double dt = now - last;
    last = now;
    if (dt > 0.1) dt = 0.1;  // clamp after stalls
    m_time += dt;
    fpsAccum += dt;
    fpsFrames++;
    if (fpsAccum >= 0.5) {
      m_fps = (int)(fpsFrames / fpsAccum);
      m_frameMs = (float)(1000.0 * fpsAccum / fpsFrames);
      fpsAccum = 0;
      fpsFrames = 0;
    }
    handleInput(dt);
    glViewport(0, 0, (GLsizei)m_width, (GLsizei)m_height);
    app::frame((float)dt);
    eglSwapBuffers(m_display, m_eglSurface);
  }
  app::shutdown();
  audioShutdown();
  destroyEgl();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void Engine::pushTouchDown(int id, float x, float y, double timeMs) {
  for (int i = 0; i < m_touchCount; i++) if (m_touches[i].id == id) return;
  if (m_touchCount >= 10) return;
  Touch& t = m_touches[m_touchCount++];
  t.id = id;
  t.pos = Vec2(x * m_width, y * m_height);
  t.start = t.pos;
  t.time = timeMs / 1000.0;
  t.startTime = t.time;
  t.down = true;
  t.moved = false;
}

void Engine::pushTouchMove(int id, float x, float y, double timeMs) {
  for (int i = 0; i < m_touchCount; i++) {
    if (m_touches[i].id != id) continue;
    Vec2 p(x * m_width, y * m_height);
    if (p.distance(m_touches[i].start) > 12.0f * m_density) m_touches[i].moved = true;
    m_touches[i].pos = p;
    m_touches[i].time = timeMs / 1000.0;
    return;
  }
}

void Engine::pushTouchUp(int id, float x, float y, double timeMs) {
  for (int i = 0; i < m_touchCount; i++) {
    if (m_touches[i].id != id) continue;
    m_touches[i].pos = Vec2(x * m_width, y * m_height);
    m_touches[i].time = timeMs / 1000.0;
    m_touches[i].down = false;
    return;
  }
}

Touch* Engine::findTouch(int id) {
  for (int i = 0; i < m_touchCount; i++) if (m_touches[i].id == id) return &m_touches[i];
  return nullptr;
}

void Engine::handleInput(double dt) {
  app::input(m_touches, m_touchCount, (float)dt);
  // retire lifted touches after the app has seen them
  for (int i = m_touchCount - 1; i >= 0; i--) {
    if (!m_touches[i].down) {
      for (int k = i; k < m_touchCount - 1; k++) m_touches[k] = m_touches[k + 1];
      m_touchCount--;
    }
  }
}

void Engine::pushText(const char* utf8) {
  size_t n = strlen(utf8);
  for (size_t i = 0; i < n && m_keyLen + 1 < sizeof m_keyQueue; i++) m_keyQueue[m_keyLen++] = utf8[i];
  m_keyQueue[m_keyLen] = 0;
  app::textInput(utf8);
}

void Engine::pushKey(int keyCode) {
  if (keyCode == 4) m_backPressed = true;  // AKEYCODE_BACK
  app::keyInput(keyCode);
}

bool Engine::backPressed() {
  bool v = m_backPressed;
  m_backPressed = false;
  return v;
}

void Engine::consumeKeys(char* out, size_t cap) {
  size_t n = m_keyLen < cap - 1 ? m_keyLen : cap - 1;
  memcpy(out, m_keyQueue, n);
  out[n] = 0;
  m_keyLen = 0;
  m_keyQueue[0] = 0;
}

void Engine::toast(const char* msg) { app::toast(msg); }

void Engine::showKeyboard() {
  if (!jni.env || !gActivity) return;
  jclass act = jni.findClass("com/nexora/game/NexoraActivity");
  if (!act) return;
  jmethodID m = jni.getMethodID(act, "showKeyboard", "()V");
  if (m) jni.callVoidMethod(gActivity, m);
  jni.deleteLocalRef(act);
}

void Engine::hideKeyboard() {
  if (!jni.env || !gActivity) return;
  jclass act = jni.findClass("com/nexora/game/NexoraActivity");
  if (!act) return;
  jmethodID m = jni.getMethodID(act, "hideKeyboard", "()V");
  if (m) jni.callVoidMethod(gActivity, m);
  jni.deleteLocalRef(act);
}

void Engine::requestMicPermission() {
  if (m_micGranted) { onPermissionResult("microphone", true); return; }
  if (!jni.env || !gActivity) return;
  jclass actCls = jni.findClass("com/nexora/game/NexoraActivity");
  if (!actCls) return;
  jmethodID req = jni.getMethodID(actCls, "requestMic", "()V");
  if (req) jni.callVoidMethod(gActivity, req);
  jni.deleteLocalRef(actCls);
}

}  // namespace nx

// ===========================================================================
// JNI bridge — thin, one job: forward to the engine.
// ===========================================================================
using namespace nx;
extern "C" jobject gActivity = nullptr;
extern "C" JavaVM* gJavaVM = nullptr;

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  if (!vm) return -1;
  JNIEnv* env = nullptr;
  typedef int (*GetEnvFn)(JavaVM*, JNIEnv**, int);
  GetEnvFn getEnv = *(GetEnvFn*)((void**)*(void**)vm + 6);
  if (!getEnv || getEnv(vm, &env, 6) != 0) return -1;
  gJavaVM = vm;
  Engine::get().init(env, vm);
  return 6;
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_setAssetManager(JNIEnv* env, jclass, jobject am) {
  jni.env = env;
  jni.vm = gJavaVM;
  setAssetManagerGlobal(jni.newGlobalRef(am));
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_setCacheDir(JNIEnv* env, jclass, jstring path) {
  char buf[512];
  int n = jni.getStringUTFLength(path);
  if (n > 0 && n < (int)sizeof buf) {
    jni.getStringUTFRegion(path, 0, n, buf);
    setCacheDirPath(buf);
  }
  jni.env = env;
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_setFilesDir(JNIEnv* env, jclass, jstring path) {
  char buf[512];
  int n = jni.getStringUTFLength(path);
  if (n > 0 && n < (int)sizeof buf) {
    jni.getStringUTFRegion(path, 0, n, buf);
    setFilesDirPath(buf);
  }
  jni.env = env;
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_surfaceCreated(JNIEnv* env, jclass, jobject surface) {
  jni.env = env;
  jni.attach();
  Engine::get().onSurfaceCreated(jni.newGlobalRef(surface));
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_setActivity(JNIEnv* env, jclass, jobject activity) {
  jni.env = env;
  if (gActivity) jni.deleteGlobalRef(gActivity);
  gActivity = jni.newGlobalRef(activity);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_surfaceChanged(JNIEnv* env, jclass, jobject surface, jint w,
                                                                        jint h, jint orientation) {
  (void)orientation;
  jni.env = env;
  Engine::get().onSurfaceChanged(surface, w, h);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_surfaceDestroyed(JNIEnv*, jclass) {
  Engine::get().onSurfaceDestroyed();
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_pause(JNIEnv*, jclass) { Engine::get().onPause(); }
JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_resume(JNIEnv*, jclass) { Engine::get().onResume(); }

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_touchDown(JNIEnv*, jclass, jint id, jfloat x, jfloat y,
                                                                   jlong time) {
  Engine::get().pushTouchDown(id, x, y, (double)time);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_touchMove(JNIEnv*, jclass, jint id, jfloat x, jfloat y,
                                                                   jlong time) {
  Engine::get().pushTouchMove(id, x, y, (double)time);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_touchUp(JNIEnv*, jclass, jint id, jfloat x, jfloat y,
                                                                 jlong time) {
  Engine::get().pushTouchUp(id, x, y, (double)time);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_key(JNIEnv*, jclass, jint code, jboolean down) {
  if (down) Engine::get().pushKey(code);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_onPermissionResult(JNIEnv*, jclass, jstring permission,
                                                                           jboolean granted) {
  char buf[64];
  int n = jni.getStringUTFLength(permission);
  if (n > 0 && n < (int)sizeof buf) jni.getStringUTFRegion(permission, 0, n, buf);
  else buf[0] = 0;
  Engine::get().setMicGranted(granted == JNI_TRUE);
  onPermissionResult(buf, granted == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_setAudioSampleRate(JNIEnv*, jclass, jint rate) {
  audioSetSampleRate(rate);
}

JNIEXPORT void JNICALL Java_com_nexora_game_NativeBridge_setAudioFramesPerBuffer(JNIEnv*, jclass, jint frames) {
  audioSetFramesPerBuffer(frames);
}

}  // extern "C"

namespace nx {

void onPermissionResult(const char* permission, bool granted) {
  if (strcmp(permission, "microphone") == 0) {
    Engine::get().setVoiceMicEnabled(granted);
    app::onMicPermission(granted);
  }
}

}  // namespace nx
