#include "gl.h"
#include "sys.h"
#include "gfx.h"

namespace nx {

extern "C" {
#define NX_DEF(ret, name, args) name##_t name##_p = nullptr;
NX_GL_FUNCS(NX_DEF)
NX_EGL_FUNCS(NX_DEF)
#undef NX_DEF
}

bool loadEGL() {
  void* h = dlopen("libEGL.so", 2 /*RTLD_NOW*/);
  if (!h) { NX_LOGE("libEGL.so not found"); return false; }
#define NX_LD(ret, name, args) name##_p = (name##_t)dlsym(h, #name); if (!name##_p) { NX_LOGE("missing egl symbol %s", #name); return false; }
  NX_EGL_FUNCS(NX_LD)
#undef NX_LD
  return true;
}

bool loadGL() {
  void* h = dlopen("libGLESv3.so", 2);
  if (!h) {
    NX_LOGW("libGLESv3.so missing, falling back to libGLESv2.so");
    h = dlopen("libGLESv2.so", 2);
  }
  if (!h) { NX_LOGE("no GLES library"); return false; }
#define NX_LD(ret, name, args) name##_p = (name##_t)dlsym(h, #name); if (!name##_p) { NX_LOGE("missing gl symbol %s", #name); return false; }
  NX_GL_FUNCS(NX_LD)
#undef NX_LD
  return true;
}

}  // namespace nx
