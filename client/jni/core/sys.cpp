#include "sys.h"
#include "gl.h"

namespace nx {
Jni jni;

// JNI constants
static const int kJNI_OK = 0;
static const int kJNI_VERSION_1_6 = 6;

struct JNINativeInterfaceApi {
  void* reserved0;
  void* reserved1;
  void* reserved2;
  void* reserved3;
  void* GetVersion;
  void* DefineClass;
  void* FindClass;
  void* FromReflectedMethod;
  void* FromReflectedField;
  void* ToReflectedMethod;
  void* GetSuperclass;
  void* IsAssignableFrom;
  void* ToReflectedField;
  void* Throw;
  void* ThrowNew;
  void* ExceptionOccurred;
  void* ExceptionDescribe;
  void* ExceptionClear;
  void* FatalError;
  void* PushLocalFrame;
  void* PopLocalFrame;
  void* NewGlobalRef;
  void* DeleteGlobalRef;
  void* DeleteLocalRef;
  void* IsSameObject;
  void* NewLocalRef;
  void* EnsureLocalCapacity;
  void* AllocObject;
  void* NewObject;
  void* NewObjectV;
  void* NewObjectA;
  void* GetObjectClass;
  void* IsInstanceOf;
  void* GetMethodID;
  void* CallObjectMethod;
  void* CallObjectMethodV;
  void* CallObjectMethodA;
  void* CallBooleanMethod;
  void* CallBooleanMethodV;
  void* CallBooleanMethodA;
  void* CallByteMethod;
  void* CallByteMethodV;
  void* CallByteMethodA;
  void* CallCharMethod;
  void* CallCharMethodV;
  void* CallCharMethodA;
  void* CallShortMethod;
  void* CallShortMethodV;
  void* CallShortMethodA;
  void* CallIntMethod;
  void* CallIntMethodV;
  void* CallIntMethodA;
  void* CallLongMethod;
  void* CallLongMethodV;
  void* CallLongMethodA;
  void* CallFloatMethod;
  void* CallFloatMethodV;
  void* CallFloatMethodA;
  void* CallDoubleMethod;
  void* CallDoubleMethodV;
  void* CallDoubleMethodA;
  void* CallVoidMethod;
  void* CallVoidMethodV;
  void* CallVoidMethodA;
  void* CallNonvirtualObjectMethod;
  void* CallNonvirtualObjectMethodV;
  void* CallNonvirtualObjectMethodA;
  void* CallNonvirtualBooleanMethod;
  void* CallNonvirtualBooleanMethodV;
  void* CallNonvirtualBooleanMethodA;
  void* CallNonvirtualByteMethod;
  void* CallNonvirtualByteMethodV;
  void* CallNonvirtualByteMethodA;
  void* CallNonvirtualCharMethod;
  void* CallNonvirtualCharMethodV;
  void* CallNonvirtualCharMethodA;
  void* CallNonvirtualShortMethod;
  void* CallNonvirtualShortMethodV;
  void* CallNonvirtualShortMethodA;
  void* CallNonvirtualIntMethod;
  void* CallNonvirtualIntMethodV;
  void* CallNonvirtualIntMethodA;
  void* CallNonvirtualLongMethod;
  void* CallNonvirtualLongMethodV;
  void* CallNonvirtualLongMethodA;
  void* CallNonvirtualFloatMethod;
  void* CallNonvirtualFloatMethodV;
  void* CallNonvirtualFloatMethodA;
  void* CallNonvirtualDoubleMethod;
  void* CallNonvirtualDoubleMethodV;
  void* CallNonvirtualDoubleMethodA;
  void* CallNonvirtualVoidMethod;
  void* CallNonvirtualVoidMethodV;
  void* CallNonvirtualVoidMethodA;
  void* GetFieldID;
  void* GetObjectField;
  void* GetBooleanField;
  void* GetByteField;
  void* GetCharField;
  void* GetShortField;
  void* GetIntField;
  void* GetLongField;
  void* GetFloatField;
  void* GetDoubleField;
  void* SetObjectField;
  void* SetBooleanField;
  void* SetByteField;
  void* SetCharField;
  void* SetShortField;
  void* SetIntField;
  void* SetLongField;
  void* SetFloatField;
  void* SetDoubleField;
  void* GetStaticMethodID;
  void* CallStaticObjectMethod;
  void* CallStaticObjectMethodV;
  void* CallStaticObjectMethodA;
  void* CallStaticBooleanMethod;
  void* CallStaticBooleanMethodV;
  void* CallStaticBooleanMethodA;
  void* CallStaticByteMethod;
  void* CallStaticByteMethodV;
  void* CallStaticByteMethodA;
  void* CallStaticCharMethod;
  void* CallStaticCharMethodV;
  void* CallStaticCharMethodA;
  void* CallStaticShortMethod;
  void* CallStaticShortMethodV;
  void* CallStaticShortMethodA;
  void* CallStaticIntMethod;
  void* CallStaticIntMethodV;
  void* CallStaticIntMethodA;
  void* CallStaticLongMethod;
  void* CallStaticLongMethodV;
  void* CallStaticLongMethodA;
  void* CallStaticFloatMethod;
  void* CallStaticFloatMethodV;
  void* CallStaticFloatMethodA;
  void* CallStaticDoubleMethod;
  void* CallStaticDoubleMethodV;
  void* CallStaticDoubleMethodA;
  void* CallStaticVoidMethod;
  void* CallStaticVoidMethodV;
  void* CallStaticVoidMethodA;
  void* GetStaticFieldID;
  void* GetStaticObjectField;
  void* GetStaticBooleanField;
  void* GetStaticByteField;
  void* GetStaticCharField;
  void* GetStaticShortField;
  void* GetStaticIntField;
  void* GetStaticLongField;
  void* GetStaticFloatField;
  void* GetStaticDoubleField;
  void* SetStaticObjectField;
  void* SetStaticBooleanField;
  void* SetStaticByteField;
  void* SetStaticCharField;
  void* SetStaticShortField;
  void* SetStaticIntField;
  void* SetStaticLongField;
  void* SetStaticFloatField;
  void* SetStaticDoubleField;
  void* NewString;
  void* GetStringLength;
  void* GetStringChars;
  void* ReleaseStringChars;
  void* NewStringUTF;
  void* GetStringUTFLength;
  void* GetStringUTFChars;
  void* ReleaseStringUTFChars;
  void* GetArrayLength;
  void* NewObjectArray;
  void* GetObjectArrayElement;
  void* SetObjectArrayElement;
  void* NewBooleanArray;
  void* NewByteArray;
  void* NewCharArray;
  void* NewShortArray;
  void* NewIntArray;
  void* NewLongArray;
  void* NewFloatArray;
  void* NewDoubleArray;
  void* GetBooleanArrayElements;
  void* GetByteArrayElements;
  void* GetCharArrayElements;
  void* GetShortArrayElements;
  void* GetIntArrayElements;
  void* GetLongArrayElements;
  void* GetFloatArrayElements;
  void* GetDoubleArrayElements;
  void* ReleaseBooleanArrayElements;
  void* ReleaseByteArrayElements;
  void* ReleaseCharArrayElements;
  void* ReleaseShortArrayElements;
  void* ReleaseIntArrayElements;
  void* ReleaseLongArrayElements;
  void* ReleaseFloatArrayElements;
  void* ReleaseDoubleArrayElements;
  void* GetBooleanArrayRegion;
  void* GetByteArrayRegion;
  void* GetCharArrayRegion;
  void* GetShortArrayRegion;
  void* GetIntArrayRegion;
  void* GetLongArrayRegion;
  void* GetFloatArrayRegion;
  void* GetDoubleArrayRegion;
  void* SetBooleanArrayRegion;
  void* SetByteArrayRegion;
  void* SetCharArrayRegion;
  void* SetShortArrayRegion;
  void* SetIntArrayRegion;
  void* SetLongArrayRegion;
  void* SetFloatArrayRegion;
  void* SetDoubleArrayRegion;
  void* RegisterNatives;
  void* UnregisterNatives;
  void* MonitorEnter;
  void* MonitorExit;
  void* GetJavaVM;
  void* GetStringRegion;
  void* GetStringUTFRegion;
  void* GetPrimitiveArrayCritical;
  void* ReleasePrimitiveArrayCritical;
  void* GetStringCritical;
  void* ReleaseStringCritical;
  void* NewWeakGlobalRef;
  void* DeleteWeakGlobalRef;
  void* ExceptionCheck;
  void* NewDirectByteBuffer;
  void* GetDirectBufferAddress;
  void* GetDirectBufferCapacity;
  void* GetObjectRefType;
};

struct JavaVMOption {
  char* optionString;
  void* extraInfo;
};
struct JavaVMInitArgs {
  int version;
  int nOptions;
  JavaVMOption* options;
  int ignoreUnrecognized;
};

struct JNIInvokeInterfaceApi {
  void* reserved0;
  void* reserved1;
  void* reserved2;
  void* DestroyJavaVM;
  void* AttachCurrentThread;
  void* DetachCurrentThread;
  void* GetEnv;
  void* AttachCurrentThreadAsDaemon;
};

static JNINativeInterfaceApi* envApi(JNIEnv* env) {
  return *(JNINativeInterfaceApi**)(env);
}

bool Jni::attach() {
  if (!vm) return false;
  if (env) return true;
  JNIInvokeInterfaceApi* api = *(JNIInvokeInterfaceApi**)vm;
  JNIEnv* e = nullptr;
  typedef int (*AttachFn)(JavaVM*, JNIEnv**, void*);
  int rc = ((AttachFn)api->AttachCurrentThread)(vm, &e, nullptr);
  if (rc != kJNI_OK) return false;
  env = e;
  return true;
}

jobject Jni::newGlobalRef(jobject o) { return ((jobject(*)(JNIEnv*, jobject))envApi(env)->NewGlobalRef)(env, o); }
void Jni::deleteLocalRef(jobject o) { ((void (*)(JNIEnv*, jobject))envApi(env)->DeleteLocalRef)(env, o); }
void Jni::deleteGlobalRef(jobject o) { ((void (*)(JNIEnv*, jobject))envApi(env)->DeleteGlobalRef)(env, o); }
jclass Jni::findClass(const char* name) { return (jclass)((jobject(*)(JNIEnv*, const char*))envApi(env)->FindClass)(env, name); }
jmethodID Jni::getStaticMethodID(jclass c, const char* name, const char* sig) {
  return ((jmethodID(*)(JNIEnv*, jclass, const char*, const char*))envApi(env)->GetStaticMethodID)(env, c, name, sig);
}
jmethodID Jni::getMethodID(jclass c, const char* name, const char* sig) {
  return ((jmethodID(*)(JNIEnv*, jclass, const char*, const char*))envApi(env)->GetMethodID)(env, c, name, sig);
}
jobject Jni::callStaticObject(jclass c, jmethodID m, ...) {
  va_list ap; va_start(ap, m);
  jobject r = ((jobject(*)(JNIEnv*, jclass, jmethodID, va_list))envApi(env)->CallStaticObjectMethodV)(env, c, m, ap);
  va_end(ap);
  return r;
}
jobject Jni::callObject(jobject o, jmethodID m, ...) {
  va_list ap; va_start(ap, m);
  jobject r = ((jobject(*)(JNIEnv*, jobject, jmethodID, va_list))envApi(env)->CallObjectMethodV)(env, o, m, ap);
  va_end(ap);
  return r;
}
bool Jni::callBooleanMethod(jobject o, jmethodID m, ...) {
  va_list ap; va_start(ap, m);
  unsigned char r = ((unsigned char(*)(JNIEnv*, jobject, jmethodID, va_list))envApi(env)->CallBooleanMethodV)(env, o, m, ap);
  va_end(ap);
  return r != 0;
}
int Jni::callIntMethod(jobject o, jmethodID m, ...) {
  va_list ap; va_start(ap, m);
  int r = ((int (*)(JNIEnv*, jobject, jmethodID, va_list))envApi(env)->CallIntMethodV)(env, o, m, ap);
  va_end(ap);
  return r;
}
void Jni::callVoidMethod(jobject o, jmethodID m, ...) {
  va_list ap; va_start(ap, m);
  ((void (*)(JNIEnv*, jobject, jmethodID, va_list))envApi(env)->CallVoidMethodV)(env, o, m, ap);
  va_end(ap);
}
void* Jni::getDirectBufferAddress(jobject buf) {
  return ((void* (*)(JNIEnv*, jobject))envApi(env)->GetDirectBufferAddress)(env, buf);
}
jstring Jni::newStringUTF(const char* s) { return (jstring)((jobject(*)(JNIEnv*, const char*))envApi(env)->NewStringUTF)(env, s); }
int Jni::getStringUTFLength(jstring s) { return ((int (*)(JNIEnv*, jstring))envApi(env)->GetStringUTFLength)(env, s); }
void Jni::getStringUTFRegion(jstring s, int start, int len, char* out) {
  ((void (*)(JNIEnv*, jstring, int, int, char*))envApi(env)->GetStringUTFRegion)(env, s, start, len, out);
}
jobject Jni::arrayGetElement(jobjectArray arr, int i, bool* isNull) {
  jobject o = ((jobject(*)(JNIEnv*, jobjectArray, int))envApi(env)->GetObjectArrayElement)(env, arr, i);
  if (isNull) *isNull = (o == nullptr);
  return o;
}
int Jni::arrayLength(jobjectArray arr) { return ((int (*)(JNIEnv*, jobjectArray))envApi(env)->GetArrayLength)(env, arr); }
jobject Jni::makeByteArray(const void* data, int len) {
  typedef void* jbyteArray;
  jbyteArray arr = ((jbyteArray(*)(JNIEnv*, int))envApi(env)->NewByteArray)(env, len);
  if (arr && data && len) {
    ((void (*)(JNIEnv*, jbyteArray, int, int, const void*))envApi(env)->SetByteArrayRegion)(env, arr, 0, len, data);
  }
  return arr;
}
jobject Jni::makeObjectArray(jclass cls, int len) {
  return ((jobject(*)(JNIEnv*, int, jclass, jobject))envApi(env)->NewObjectArray)(env, len, cls, nullptr);
}
void Jni::setObjectArrayElement(jobjectArray arr, int i, jobject obj) {
  ((void (*)(JNIEnv*, jobjectArray, int, jobject))envApi(env)->SetObjectArrayElement)(env, arr, i, obj);
}

// ---------------------------------------------------------------------------
// assets / files
// ---------------------------------------------------------------------------
static jobject gAssetManager = nullptr;  // global ref to android.content.res.AssetManager
static char gCacheDir[512] = "/data/local/tmp";
static char gFilesDir[512] = "/data/local/tmp";

const char* cacheDir() { return gCacheDir; }
const char* filesDir() { return gFilesDir; }

void setAssetManagerGlobal(jobject am) { gAssetManager = am; }
void setCacheDirPath(const char* p) { strncpy(gCacheDir, p, sizeof gCacheDir - 1); }
void setFilesDirPath(const char* p) { strncpy(gFilesDir, p, sizeof gFilesDir - 1); }

uint64_t fnv1a(const void* data, size_t size) {
  const unsigned char* p = (const unsigned char*)data;
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < size; i++) { h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

bool readAsset(const char* path, void** outData, size_t* outSize) {
  *outData = nullptr;
  *outSize = 0;
  if (!gAssetManager || !jni.env) return false;
  jclass amClass = jni.findClass("android/content/res/AssetManager");
  if (!amClass) return false;
  // open(path, ACCESS_STREAMING=2) -> InputStream
  jmethodID openM = jni.getMethodID(amClass, "open", "(Ljava/lang/String;I)Ljava/io/InputStream;");
  if (!openM) { jni.deleteLocalRef(amClass); return false; }
  jstring jpath = jni.newStringUTF(path);
  jobject stream = jni.callObject(gAssetManager, openM, jpath, 2);
  jni.deleteLocalRef(jpath);
  jni.deleteLocalRef(amClass);
  if (!stream) return false;

  jclass isClass = jni.findClass("java/io/InputStream");
  jmethodID readM = jni.getMethodID(isClass, "read", "([BII)I");
  jmethodID closeM = jni.getMethodID(isClass, "close", "()V");
  jclass baosClass = jni.findClass("java/io/ByteArrayOutputStream");
  jmethodID baosCtor = jni.getMethodID(baosClass, "<init>", "()V");
  jmethodID writeM = jni.getMethodID(baosClass, "write", "([BII)V");
  jmethodID toByteArrayM = jni.getMethodID(baosClass, "toByteArray", "()[B");
  jobject baos = ((jobject(*)(JNIEnv*, jclass, jmethodID, ...))envApi(jni.env)->NewObject)(
      jni.env, baosClass, baosCtor);
  if (!baos) { jni.callVoidMethod(stream, closeM); jni.deleteLocalRef(stream); return false; }

  const int CHUNK = 16384;
  typedef void* jbyteArray_t;
  jbyteArray_t buf = ((jbyteArray_t(*)(JNIEnv*, int))envApi(jni.env)->NewByteArray)(jni.env, CHUNK);
  for (;;) {
    int n = jni.callIntMethod(stream, readM, buf, 0, CHUNK);
    if (n <= 0) break;
    jni.callVoidMethod(baos, writeM, buf, 0, n);
  }
  jni.callVoidMethod(stream, closeM);
  jni.deleteLocalRef(stream);
  jni.deleteLocalRef(buf);

  jbyteArray_t result = (jbyteArray_t)jni.callObject(baos, toByteArrayM);
  jni.deleteLocalRef(baos);
  jni.deleteLocalRef(isClass);
  jni.deleteLocalRef(baosClass);
  if (!result) return false;
  int len = ((int (*)(JNIEnv*, jbyteArray_t))envApi(jni.env)->GetArrayLength)(jni.env, result);
  unsigned char* data = (unsigned char*)malloc(len + 1);
  ((void (*)(JNIEnv*, jbyteArray_t, int, int, void*))envApi(jni.env)->GetByteArrayRegion)(jni.env, result, 0, len, data);
  data[len] = 0;
  jni.deleteLocalRef(result);
  *outData = data;
  *outSize = (size_t)len;
  return true;
}

void freeAsset(void* p) { if (p) free(p); }

static const int kO_WRONLY = 1, kO_CREAT = 64, kO_TRUNC = 512;
#ifndef F_OK
#define F_OK 0
#endif

bool writeFile(const char* path, const void* data, size_t size) {
  int fd = open(path, kO_WRONLY | kO_CREAT | kO_TRUNC, 0644);
  if (fd < 0) return false;
  const char* p = (const char*)data;
  size_t left = size;
  while (left > 0) {
    long n = write(fd, p, left);
    if (n <= 0) { close(fd); return false; }
    p += n;
    left -= (size_t)n;
  }
  close(fd);
  return true;
}

bool readFile(const char* path, void** outData, size_t* outSize) {
  *outData = nullptr;
  *outSize = 0;
  int fd = open(path, O_RDONLY);
  if (fd < 0) return false;
  long size = lseek(fd, 0, 2 /*SEEK_END*/);
  if (size < 0) { close(fd); return false; }
  lseek(fd, 0, 0 /*SEEK_SET*/);
  void* data = malloc((size_t)size + 1);
  size_t got = 0;
  while (got < (size_t)size) {
    long n = read(fd, (char*)data + got, (size_t)size - got);
    if (n <= 0) break;
    got += (size_t)n;
  }
  close(fd);
  ((char*)data)[got] = 0;
  *outData = data;
  *outSize = got;
  return true;
}

bool fileExists(const char* path) { return access(path, F_OK) == 0; }

// ---------------------------------------------------------------------------
// render-thread job queue
// ---------------------------------------------------------------------------
static const int kMaxJobs = 256;
static Job gJobs[kMaxJobs];
static volatile int gJobHead = 0, gJobTail = 0;
static volatile int gJobMutex = 0;

static void lockJobs() {
  while (__atomic_exchange_n(&gJobMutex, 1, __ATOMIC_ACQUIRE)) { }
}
static void unlockJobs() { __atomic_store_n(&gJobMutex, 0, __ATOMIC_RELEASE); }

void pushJob(const Job& j) {
  lockJobs();
  int next = (gJobHead + 1) % kMaxJobs;
  if (next != gJobTail) {
    gJobs[gJobHead] = j;
    gJobHead = next;
  }
  unlockJobs();
}
bool popJob(Job* out) {
  lockJobs();
  bool ok = false;
  if (gJobHead != gJobTail) {
    *out = gJobs[gJobTail];
    gJobTail = (gJobTail + 1) % kMaxJobs;
    ok = true;
  }
  unlockJobs();
  return ok;
}
int pendingJobs() {
  lockJobs();
  int n = (gJobHead - gJobTail + kMaxJobs) % kMaxJobs;
  unlockJobs();
  return n;
}

}  // namespace nx
