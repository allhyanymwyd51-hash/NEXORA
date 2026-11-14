// NEXORA engine — freestanding C++ runtime glue.
//
// The engine links with -nostdlib against only the platform's libc/libm/libdl,
// so the handful of C++ runtime symbols the compiler emits for statics,
// globals and `new` are provided here. There is deliberately no exception
// support (-fno-exceptions): allocation failure is treated as fatal, exactly
// like an unhandled bad_alloc would be.
#include "sys.h"

extern "C" void abort(void);

void* __dso_handle = (void*)&__dso_handle;

namespace {
void* allocate(size_t size) {
  void* p = malloc(size ? size : 1);
  if (!p) {
    NX_LOGE("out of memory allocating %u bytes", (unsigned)size);
    abort();
  }
  return p;
}
}  // namespace

void* operator new(size_t size) { return allocate(size); }
void* operator new[](size_t size) { return allocate(size); }
void operator delete(void* p) noexcept { if (p) free(p); }
void operator delete[](void* p) noexcept { if (p) free(p); }
void operator delete(void* p, size_t) noexcept { if (p) free(p); }
void operator delete[](void* p, size_t) noexcept { if (p) free(p); }

extern "C" {

int __cxa_atexit(void (*fn)(void*), void* arg, void* dso) {
  // static destructors never run in the engine; they only exist for globals with
  // non-trivial teardown, which the engine deliberately avoids
  (void)fn;
  (void)arg;
  (void)dso;
  return 0;
}

int __cxa_guard_acquire(long long* guard) {
  // single-threaded first use at load time; guard is zero until the init runs
  return *(volatile long long*)guard == 0 ? 1 : 0;
}

void __cxa_guard_release(long long* guard) { *(volatile long long*)guard = 1; }

void __cxa_pure_virtual(void) {
  NX_LOGE("pure virtual call");
  abort();
}

}  // extern "C"

// ---------------------------------------------------------------------------
// Out-of-line atomics for armv8-a (devices without LSE). LLVM lowers the
// acquire/release atomics the engine uses on its worker queues to these helpers
// when targeting plain armv8-a, and bionic's exports vary by platform version —
// so the engine carries its own implementations.
// ---------------------------------------------------------------------------
#if defined(__aarch64__)
extern "C" {

int __aarch64_swp4_acq(volatile int* ptr, int value) {
  int expected, result = value;
  do {
    expected = *ptr;
    __asm__ __volatile__("ldaxr %w0, [%1]\n stlxr w2, %w3, [%1]\n cbnz w2, 1f\n 1:"
                         : "=&r"(result), "+r"(ptr)
                         : "r"(0), "r"(value)
                         : "memory", "cc");
    return expected;
  } while (0);
  return expected;
}

int __aarch64_swp4_rel(int* ptr, int value) {
  int expected;
  __asm__ __volatile__("stlr %w1, [%0]"::"r"(ptr), "r"(value) : "memory");
  expected = value;
  return expected;
}

int __aarch64_ldadd4_acq(int* ptr, int value) {
  int old;
  __asm__ __volatile__("ldaxr %w0, [%1]\n add %w0, %w0, %w2\n stlxr w3, %w0, [%1]"
                       : "=&r"(old), "+r"(ptr)
                       : "r"(value)
                       : "memory", "cc");
  return old;
}

int __aarch64_cas4_acq(int* ptr, int expected, int desired) {
  int old;
  __asm__ __volatile__("ldaxr %w0, [%1]\n cmp %w0, %w2\n b.ne 1f\n stlxr w3, %w4, [%1]\n 1:"
                       : "=&r"(old), "+r"(ptr)
                       : "r"(expected), "r"(0), "r"(desired)
                       : "memory", "cc");
  return old;
}

int __aarch64_cas4_relax(int* ptr, int expected, int desired) {
  int old;
  __asm__ __volatile__("ldxr %w0, [%1]\n cmp %w0, %w2\n b.ne 1f\n stxr w3, %w4, [%1]\n 1:"
                       : "=&r"(old), "+r"(ptr)
                       : "r"(expected), "r"(0), "r"(desired)
                       : "memory", "cc");
  return old;
}

}  // extern "C"
#endif
