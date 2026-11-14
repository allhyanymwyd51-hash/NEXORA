// NEXORA engine — freestanding <string.h> (no NDK sysroot available to this build).
// Declarations only: every symbol resolves against the platform's libc.so at load time.
#pragma once
#include <stddef.h>

extern "C" {
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* dst, int c, size_t n);
int memcmp(const void* a, const void* b, size_t n);
void* memchr(const void* s, int c, size_t n);
size_t strlen(const char* s);
size_t strnlen(const char* s, size_t max);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
int strcasecmp(const char* a, const char* b);
int strncasecmp(const char* a, const char* b, size_t n);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, size_t n);
char* strcat(char* dst, const char* src);
char* strncat(char* dst, const char* src, size_t n);
char* strstr(const char* haystack, const char* needle);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strdup(const char* s);
char* strtok(char* s, const char* delim);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);
char* strerror(int errnum);
}
