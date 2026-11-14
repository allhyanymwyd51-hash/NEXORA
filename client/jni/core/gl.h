// NEXORA engine — OpenGL ES 3.0 / EGL declarations (self-contained, no NDK headers).
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef int8_t khronos_int8_t;
typedef uint8_t khronos_uint8_t;
typedef int32_t khronos_int32_t;
typedef uint32_t khronos_uint32_t;
typedef float khronos_float_t;
typedef int64_t khronos_int64_t;
typedef uint64_t khronos_uint64_t;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef khronos_int8_t GLbyte;
typedef khronos_uint8_t GLubyte;
typedef short GLshort;
typedef unsigned short GLushort;
typedef int GLint;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef float GLclampf;
typedef khronos_int32_t GLfixed;
typedef intptr_t GLintptr;
typedef ptrdiff_t GLsizeiptr;
typedef char GLchar;
typedef khronos_int64_t GLint64;
typedef khronos_uint64_t GLuint64;

#define GL_NONE 0
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_FUNC_ADD 0x8006
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_RGBA8 0x8058
#define GL_RGB8 0x8051
#define GL_R8 0x8229
#define GL_RED 0x1903
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_DEPTH_FUNC 0x0B74
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_ALWAYS 0x0207
#define GL_EQUAL 0x0202
#define GL_SRC1_ALPHA 0x8589
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_CCW 0x0901
#define GL_CW 0x0900
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_VERSION 0x1F02
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_EXTENSIONS 0x1F03
#define GL_MAX_SAMPLES 0x8D57
#define GL_NUM_EXTENSIONS 0x821D
#define GL_VERSION_ES_CM_1_0 1

// EGL
typedef void* EGLNativeDisplayType;
typedef void* EGLNativeWindowType;
typedef int32_t EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef void* EGLConfig;
typedef void* EGLClientBuffer;

#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_FALSE 0
#define EGL_TRUE 1
#define EGL_NONE 0x3038
#define EGL_SWAP_BEHAVIOR 0x3093
#define EGL_BUFFER_PRESERVED 0x3094
#define EGL_BUFFER_DESTROYED 0x3095
#define EGL_ALPHA_SIZE 0x3021
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_OPENGL_ES3_BIT 0x0040
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_NATIVE_VISUAL_ID 0x302E
#define EGL_SAMPLES 0x3032
#define EGL_NATIVE_WINDOW 0x302E
#define EGL_ALPHA_FORMAT 0x3088
#define EGL_SRGB8_ALPHA8_ANDROID 0x0008

// ---- function pointers (resolved by dlsym in gl.cpp) ----
#define NX_GL_FUNCS(X) \
  X(void, glClear, (GLbitfield)) \
  X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
  X(void, glViewport, (GLint, GLint, GLsizei, GLsizei)) \
  X(void, glScissor, (GLint, GLint, GLsizei, GLsizei)) \
  X(void, glEnable, (GLenum)) \
  X(void, glDisable, (GLenum)) \
  X(void, glBlendFunc, (GLenum, GLenum)) \
  X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
  X(void, glBlendEquation, (GLenum)) \
  X(void, glDepthFunc, (GLenum)) \
  X(void, glDepthMask, (GLboolean)) \
  X(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
  X(void, glCullFace, (GLenum)) \
  X(void, glFrontFace, (GLenum)) \
  X(void, glGenTextures, (GLsizei, GLuint*)) \
  X(void, glDeleteTextures, (GLsizei, const GLuint*)) \
  X(void, glBindTexture, (GLenum, GLuint)) \
  X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
  X(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)) \
  X(void, glTexParameteri, (GLenum, GLenum, GLint)) \
  X(void, glTexParameterf, (GLenum, GLenum, GLfloat)) \
  X(void, glGenerateMipmap, (GLenum)) \
  X(void, glPixelStorei, (GLenum, GLint)) \
  X(void, glActiveTexture, (GLenum)) \
  X(void, glGenBuffers, (GLsizei, GLuint*)) \
  X(void, glDeleteBuffers, (GLsizei, const GLuint*)) \
  X(void, glBindBuffer, (GLenum, GLuint)) \
  X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
  X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void*)) \
  X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
  X(void, glEnableVertexAttribArray, (GLuint)) \
  X(void, glDisableVertexAttribArray, (GLuint)) \
  X(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
  X(void, glDrawElements, (GLenum, GLsizei, GLenum, const void*)) \
  X(GLuint, glCreateShader, (GLenum)) \
  X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
  X(void, glCompileShader, (GLuint)) \
  X(void, glGetShaderiv, (GLuint, GLenum, GLint*)) \
  X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
  X(void, glDeleteShader, (GLuint)) \
  X(GLuint, glCreateProgram, (void)) \
  X(void, glAttachShader, (GLuint, GLuint)) \
  X(void, glLinkProgram, (GLuint)) \
  X(void, glGetProgramiv, (GLuint, GLenum, GLint*)) \
  X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
  X(void, glUseProgram, (GLuint)) \
  X(void, glDeleteProgram, (GLuint)) \
  X(GLint, glGetAttribLocation, (GLuint, const GLchar*)) \
  X(GLint, glGetUniformLocation, (GLuint, const GLchar*)) \
  X(void, glUniform1i, (GLint, GLint)) \
  X(void, glUniform1f, (GLint, GLfloat)) \
  X(void, glUniform2f, (GLint, GLfloat, GLfloat)) \
  X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat)) \
  X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
  X(void, glUniform2fv, (GLint, GLsizei, const GLfloat*)) \
  X(void, glUniform3fv, (GLint, GLsizei, const GLfloat*)) \
  X(void, glUniform4fv, (GLint, GLsizei, const GLfloat*)) \
  X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*)) \
  X(const GLubyte*, glGetString, (GLenum)) \
  X(void, glGetIntegerv, (GLenum, GLint*)) \
  X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
  X(GLenum, glGetError, (void)) \
  X(void, glPolygonOffset, (GLfloat, GLfloat)) \
  X(void, glGenFramebuffers, (GLsizei, GLuint*)) \
  X(void, glBindFramebuffer, (GLenum, GLuint)) \
  X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
  X(void, glCheckFramebufferStatus, (GLenum)) \
  X(void, glGenRenderbuffers, (GLsizei, GLuint*)) \
  X(void, glBindRenderbuffer, (GLenum, GLuint)) \
  X(void, glRenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
  X(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
  X(void, glDeleteFramebuffers, (GLsizei, const GLuint*)) \
  X(void, glDeleteRenderbuffers, (GLsizei, const GLuint*)) \
  X(void, glLineWidth, (GLfloat)) \
  X(void, glFinish, (void)) \
  X(void, glFlush, (void))

// The function pointers carry C linkage so the loader (gl.cpp) and the call
// sites agree on one symbol, whatever namespace the includer sits in.
extern "C" {
#define NX_DECL(ret, name, args) typedef ret(*name##_t) args; extern name##_t name##_p;
NX_GL_FUNCS(NX_DECL)
#undef NX_DECL
}
#define glClear glClear_p
#define glClearColor glClearColor_p
#define glViewport glViewport_p
#define glScissor glScissor_p
#define glEnable glEnable_p
#define glDisable glDisable_p
#define glBlendFunc glBlendFunc_p
#define glBlendFuncSeparate glBlendFuncSeparate_p
#define glBlendEquation glBlendEquation_p
#define glDepthFunc glDepthFunc_p
#define glDepthMask glDepthMask_p
#define glColorMask glColorMask_p
#define glCullFace glCullFace_p
#define glFrontFace glFrontFace_p
#define glGenTextures glGenTextures_p
#define glDeleteTextures glDeleteTextures_p
#define glBindTexture glBindTexture_p
#define glTexImage2D glTexImage2D_p
#define glTexSubImage2D glTexSubImage2D_p
#define glTexParameteri glTexParameteri_p
#define glTexParameterf glTexParameterf_p
#define glGenerateMipmap glGenerateMipmap_p
#define glPixelStorei glPixelStorei_p
#define glActiveTexture glActiveTexture_p
#define glGenBuffers glGenBuffers_p
#define glDeleteBuffers glDeleteBuffers_p
#define glBindBuffer glBindBuffer_p
#define glBufferData glBufferData_p
#define glBufferSubData glBufferSubData_p
#define glVertexAttribPointer glVertexAttribPointer_p
#define glEnableVertexAttribArray glEnableVertexAttribArray_p
#define glDisableVertexAttribArray glDisableVertexAttribArray_p
#define glDrawArrays glDrawArrays_p
#define glDrawElements glDrawElements_p
#define glCreateShader glCreateShader_p
#define glShaderSource glShaderSource_p
#define glCompileShader glCompileShader_p
#define glGetShaderiv glGetShaderiv_p
#define glGetShaderInfoLog glGetShaderInfoLog_p
#define glDeleteShader glDeleteShader_p
#define glCreateProgram glCreateProgram_p
#define glAttachShader glAttachShader_p
#define glLinkProgram glLinkProgram_p
#define glGetProgramiv glGetProgramiv_p
#define glGetProgramInfoLog glGetProgramInfoLog_p
#define glUseProgram glUseProgram_p
#define glDeleteProgram glDeleteProgram_p
#define glGetAttribLocation glGetAttribLocation_p
#define glGetUniformLocation glGetUniformLocation_p
#define glUniform1i glUniform1i_p
#define glUniform1f glUniform1f_p
#define glUniform2f glUniform2f_p
#define glUniform3f glUniform3f_p
#define glUniform4f glUniform4f_p
#define glUniform2fv glUniform2fv_p
#define glUniform3fv glUniform3fv_p
#define glUniform4fv glUniform4fv_p
#define glUniformMatrix4fv glUniformMatrix4fv_p
#define glGetString glGetString_p
#define glGetIntegerv glGetIntegerv_p
#define glReadPixels glReadPixels_p
#define glGetError glGetError_p
#define glPolygonOffset glPolygonOffset_p
#define glGenFramebuffers glGenFramebuffers_p
#define glBindFramebuffer glBindFramebuffer_p
#define glFramebufferTexture2D glFramebufferTexture2D_p
#define glCheckFramebufferStatus glCheckFramebufferStatus_p
#define glGenRenderbuffers glGenRenderbuffers_p
#define glBindRenderbuffer glBindRenderbuffer_p
#define glRenderbufferStorage glRenderbufferStorage_p
#define glFramebufferRenderbuffer glFramebufferRenderbuffer_p
#define glDeleteFramebuffers glDeleteFramebuffers_p
#define glDeleteRenderbuffers glDeleteRenderbuffers_p
#define glLineWidth glLineWidth_p
#define glFinish glFinish_p
#define glFlush glFlush_p

#define NX_EGL_FUNCS(X) \
  X(EGLDisplay, eglGetDisplay, (EGLNativeDisplayType)) \
  X(EGLBoolean, eglInitialize, (EGLDisplay, EGLint*, EGLint*)) \
  X(EGLBoolean, eglChooseConfig, (EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*)) \
  X(EGLSurface, eglCreateWindowSurface, (EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*)) \
  X(EGLContext, eglCreateContext, (EGLDisplay, EGLConfig, EGLContext, const EGLint*)) \
  X(EGLBoolean, eglMakeCurrent, (EGLDisplay, EGLSurface, EGLSurface, EGLContext)) \
  X(EGLBoolean, eglSwapBuffers, (EGLDisplay, EGLSurface)) \
  X(EGLBoolean, eglDestroySurface, (EGLDisplay, EGLSurface)) \
  X(EGLBoolean, eglDestroyContext, (EGLDisplay, EGLContext)) \
  X(EGLBoolean, eglTerminate, (EGLDisplay)) \
  X(EGLint, eglGetError, (void)) \
  X(EGLBoolean, eglBindAPI, (EGLenum)) \
  X(EGLBoolean, eglQuerySurface, (EGLDisplay, EGLSurface, EGLint, EGLint*)) \
  X(EGLBoolean, eglSurfaceAttrib, (EGLDisplay, EGLSurface, EGLint, EGLint)) \
  X(EGLBoolean, eglReleaseThread, (void))

extern "C" {
#define NX_DECL2(ret, name, args) typedef ret(*name##_t) args; extern name##_t name##_p;
NX_EGL_FUNCS(NX_DECL2)
#undef NX_DECL2
}
#define eglGetDisplay eglGetDisplay_p
#define eglInitialize eglInitialize_p
#define eglChooseConfig eglChooseConfig_p
#define eglCreateWindowSurface eglCreateWindowSurface_p
#define eglCreateContext eglCreateContext_p
#define eglMakeCurrent eglMakeCurrent_p
#define eglSwapBuffers eglSwapBuffers_p
#define eglDestroySurface eglDestroySurface_p
#define eglDestroyContext eglDestroyContext_p
#define eglTerminate eglTerminate_p
#define eglGetError eglGetError_p
#define eglBindAPI eglBindAPI_p
#define eglQuerySurface eglQuerySurface_p
#define eglSurfaceAttrib eglSurfaceAttrib_p
#define eglReleaseThread eglReleaseThread_p

namespace nx {
bool loadGL();
bool loadEGL();
}
