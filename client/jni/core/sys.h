// NEXORA engine — platform shims: logging, dlopen, JNI, AAudio, time, obb-free asset IO.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>


// ---------------------------------------------------------------------------
// libc / libm / libdl imports (declared by hand; linked against the NDK stubs at runtime)
// ---------------------------------------------------------------------------
extern "C" {
typedef void* JavaVM;
typedef void* JNIEnv;
typedef void* jobject;
typedef void* jclass;
typedef void* jstring;
typedef void* jmethodID;
typedef void* jfieldID;
typedef void* jobjectArray;
typedef int32_t jint;
typedef int64_t jlong;
typedef float jfloat;
typedef double jdouble;
typedef uint8_t jboolean;
typedef uint16_t jchar;
typedef int8_t jbyte;
typedef int16_t jshort;
typedef void* jarray;
typedef void* jbyteArray;
typedef void* jintArray;
typedef void* jfloatArray;
typedef void* jbooleanArray;
#define JNI_TRUE 1
#define JNI_FALSE 0

struct JNINativeInterface;
struct JNIInvokeInterface;

void* dlopen(const char* filename, int flag);
void* dlsym(void* handle, const char* symbol);
char* dlerror(void);

int __android_log_print(int prio, const char* tag, const char* fmt, ...);
#ifndef NEX_HOST_TESTS
// stdio subset (declared here because the engine never links a stdio implementation)
int snprintf(char* buf, size_t cap, const char* fmt, ...);
int vsnprintf(char* buf, size_t cap, const char* fmt, va_list ap);
int sprintf(char* buf, const char* fmt, ...);
int sscanf(const char* s, const char* fmt, ...);
#endif
#define NX_LOGI(...) __android_log_print(4, "NEXORA", __VA_ARGS__)
#define NX_LOGW(...) __android_log_print(5, "NEXORA", __VA_ARGS__)
#define NX_LOGE(...) __android_log_print(6, "NEXORA", __VA_ARGS__)

// The engine normally compiles freestanding, where these types do not exist yet;
// NEX_HOST_TESTS lets a host build reuse the same sources against glibc.
#ifndef NEX_HOST_TESTS
struct timespec { long tv_sec; long tv_nsec; };
struct timeval { long tv_sec; long tv_usec; };
struct tm {
  int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
  long tm_gmtoff;
  const char* tm_zone;
};
#else
#include <cstdio>
#include <ctime>
#include <unistd.h>
#endif
#ifndef NEX_HOST_TESTS
int clock_gettime(int clockid, struct timespec* ts);
long time(long* tloc);
struct tm* localtime_r(const long* t, struct tm* out);
struct tm* gmtime_r(const long* t, struct tm* out);
#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 0
#endif

int usleep(unsigned int usec);
unsigned int sleep(unsigned int s);

int open(const char* path, int flags, ...);
int access(const char* path, int mode);
int close(int fd);
long read(int fd, void* buf, size_t count);
long write(int fd, const void* buf, size_t count);
long lseek(int fd, long offset, int whence);
#define O_RDONLY 0

int pthread_mutex_init(void* mutex, const void* attr);
int pthread_mutex_lock(void* mutex);
int pthread_mutex_unlock(void* mutex);
int pthread_mutex_destroy(void* mutex);
int pthread_create(unsigned long* thread, const void* attr, void* (*start)(void*), void* arg);
int pthread_join(unsigned long thread, void** retval);
unsigned long pthread_self();
#define NX_MUTEX_SIZE 40

// C++ runtime glue the freestanding build provides itself (see core/cxxstubs.cpp)
int __cxa_atexit(void (*fn)(void*), void* arg, void* dso);
int __cxa_guard_acquire(long long* guard);
void __cxa_guard_release(long long* guard);

// AAudio (Android 8+) — used for the mixed output stream and mic capture
typedef struct AAudioStream AAudioStream;
typedef int32_t aaudio_result_t;
typedef struct AAudioStreamBuilder AAudioStreamBuilder;
typedef void (*aaudio_data_callback_t)(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);
typedef void (*aaudio_error_callback_t)(AAudioStream* stream, void* userData, aaudio_result_t error);

aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder** builder);
void AAudioStreamBuilder_setDirection(AAudioStreamBuilder*, int direction);
void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder*, int mode);
void AAudioStreamBuilder_setFormat(AAudioStreamBuilder*, int format);
void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder*, int32_t channels);
void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder*, int32_t sampleRate);
void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder*, aaudio_data_callback_t cb, void* userData);
void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder*, aaudio_error_callback_t cb, void* userData);
void AAudioStreamBuilder_setSharingMode(AAudioStreamBuilder*, int mode);
aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder*, AAudioStream** stream);
void AAudioStreamBuilder_delete(AAudioStreamBuilder*);
aaudio_result_t AAudioStream_requestStart(AAudioStream*);
aaudio_result_t AAudioStream_requestStop(AAudioStream*);
aaudio_result_t AAudioStream_close(AAudioStream*);
aaudio_result_t AAudioStream_read(AAudioStream* stream, void* buffer, int32_t numFrames, int64_t timeoutNanos);
int32_t AAudioStream_getSampleRate(AAudioStream*);
int32_t AAudioStream_getChannelCount(AAudioStream*);
int32_t AAudioStream_getFramesPerBurst(AAudioStream*);
aaudio_result_t AAudioStream_setBufferSizeInFrames(AAudioStream*, int32_t);
#define AAUDIO_DIRECTION_OUTPUT 0
#define AAUDIO_DIRECTION_INPUT 1
#define AAUDIO_FORMAT_PCM_I16 1
#define AAUDIO_FORMAT_PCM_FLOAT 2
#define AAUDIO_PERFORMANCE_MODE_LOW_LATENCY 2
#define AAUDIO_SHARING_MODE_SHARED 0
#define AAUDIO_OK 0

struct ANativeWindow;
int32_t ANativeWindow_setBuffersGeometry(struct ANativeWindow* window, int32_t width, int32_t height, int32_t format);
}

namespace nx {

inline double nowSeconds() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// Full JNI environment access (struct layouts are fixed by the JNI spec).
struct Jni {
  JavaVM* vm = nullptr;
  JNIEnv* env = nullptr;
  bool attach();
  // object management
  jobject newGlobalRef(jobject o);
  void deleteLocalRef(jobject o);
  void deleteGlobalRef(jobject o);
  // classes / calls
  jclass findClass(const char* name);
  jmethodID getStaticMethodID(jclass c, const char* name, const char* sig);
  jmethodID getMethodID(jclass c, const char* name, const char* sig);
  jobject callStaticObject(jclass c, jmethodID m, ...);
  jobject callObject(jobject o, jmethodID m, ...);
  bool callBooleanMethod(jobject o, jmethodID m, ...);
  int callIntMethod(jobject o, jmethodID m, ...);
  void callVoidMethod(jobject o, jmethodID m, ...);
  void* getDirectBufferAddress(jobject buf);
  jstring newStringUTF(const char* s);
  int getStringUTFLength(jstring s);
  void getStringUTFRegion(jstring s, int start, int len, char* out);
  jobject arrayGetElement(jobjectArray arr, int i, bool* isNull);
  int arrayLength(jobjectArray arr);
  jobject makeByteArray(const void* data, int len);
  jobject makeObjectArray(jclass cls, int len);
  void setObjectArrayElement(jobjectArray arr, int i, jobject obj);
};

extern Jni jni;

// ---- filesystem ----
bool readAsset(const char* path, void** outData, size_t* outSize);
void freeAsset(void* p);
bool writeFile(const char* path, const void* data, size_t size);
bool readFile(const char* path, void** outData, size_t* outSize);
bool fileExists(const char* path);
const char* cacheDir();
const char* filesDir();
// simple 64-bit checksum used for asset caching
uint64_t fnv1a(const void* data, size_t size);

// Growable byte/text buffer
struct Buf {
  char* data = nullptr;
  size_t len = 0, cap = 0;
  void reserve(size_t n) {
    if (len + n + 1 <= cap) return;
    size_t nc = cap ? cap : 256;
    while (nc < len + n + 1) nc *= 2;
    char* nd = (char*)realloc(data, nc);
    data = nd;
    cap = nc;
  }
  void append(const char* s, size_t n) {
    reserve(n);
    memcpy(data + len, s, n);
    len += n;
    data[len] = 0;
  }
  void append(const char* s) { append(s, strlen(s)); }
  void appendf(const char* fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    append(tmp, strlen(tmp));
  }
  void clear() { len = 0; if (data) data[0] = 0; }
  ~Buf() { if (data) free(data); }
  Buf() {}
  Buf(const Buf&) = delete;
  Buf& operator=(const Buf&) = delete;
};

// Growable typed vector
template <typename T>
struct Array {
  T* items = nullptr;
  int count = 0, cap = 0;
  void reserve(int n) {
    if (n <= cap) return;
    int nc = cap ? cap * 2 : 16;
    while (nc < n) nc *= 2;
    items = (T*)realloc(items, sizeof(T) * nc);
    cap = nc;
  }
  void push(const T& v) {
    reserve(count + 1);
    items[count++] = v;
  }
  void clear() { count = 0; }
  T& operator[](int i) { return items[i]; }
  const T& operator[](int i) const { return items[i]; }
  bool empty() const { return count == 0; }
  T& last() { return items[count - 1]; }
  void removeAt(int i) {
    if (i < 0 || i >= count) return;
    for (int k = i; k < count - 1; k++) items[k] = items[k + 1];
    count--;
  }
  ~Array() { if (items) free(items); }
  Array() {}
  Array(const Array&) = delete;
  Array& operator=(const Array&) = delete;
};

// Render-thread job queue (network completions are handed over here)
struct Job {
  int type;
  int id;
  int code;
  char* payload;
  size_t len;
};
void pushJob(const Job& j);
bool popJob(Job* out);
int pendingJobs();

}  // namespace nx
