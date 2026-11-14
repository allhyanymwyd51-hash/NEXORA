// host-only stubs for the GL entry points the font atlas touches
#include "core/gl.h"
extern "C" {
#define NX_DEF(ret, name, args) name##_t name##_p = nullptr;
NX_GL_FUNCS(NX_DEF)
NX_EGL_FUNCS(NX_DEF)
#undef NX_DEF
}
