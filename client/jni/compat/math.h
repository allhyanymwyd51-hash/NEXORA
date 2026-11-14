// NEXORA engine — freestanding <math.h>.
//
// The build targets Android without an NDK sysroot, so the C library headers are
// provided here (declarations only). All symbols resolve against the platform's
// libc.so/libm.so at load time; Zig emits calls through the PLT.
#pragma once

#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_TAU 6.28318530717958647692
#define M_E 2.7182818284590452354
#define M_SQRT2 1.41421356237309504880
#define HUGE_VALF __builtin_huge_valf()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")

extern "C" {
// ---- basic ----
float fabsf(float x);
double fabs(double x);
float fmodf(float x, float y);
double fmod(double x, double y);
float sqrtf(float x);
double sqrt(double x);
float cbrtf(float x);
double cbrt(double x);
float powf(float x, float y);
double pow(double x, double y);
float expf(float x);
double exp(double x);
float exp2f(float x);
float logf(float x);
double log(double x);
float log2f(float x);
float log10f(float x);
float hypotf(float x, float y);
float ceilf(float x);
double ceil(double x);
float floorf(float x);
double floor(double x);
float roundf(float x);
double round(double x);
float truncf(float x);
double trunc(double x);
long lroundf(float x);
long lrintf(float x);

// ---- trigonometry ----
float sinf(float x);
double sin(double x);
float cosf(float x);
double cos(double x);
float tanf(float x);
double tan(double x);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);
double atan2(double y, double x);
void sincosf(float x, float* s, float* c);

// ---- classification ----
int __isinff(float);
int __isnanf(float);
#define isfinite(x) __builtin_isfinite(x)
#define isinf(x) __builtin_isinf(x)
#define isnan(x) __builtin_isnan(x)
#define signbit(x) __builtin_signbit(x)

// ---- float helpers used by the engine ----
float fminf(float a, float b);
float fmaxf(float a, float b);
double fmin(double a, double b);
double fmax(double a, double b);
float fmaf(float a, float b, float c);
float copysignf(float mag, float sign);
float ldexpf(float x, int e);
float frexpf(float x, int* e);
}
