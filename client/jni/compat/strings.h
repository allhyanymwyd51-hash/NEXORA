// NEXORA engine — freestanding <strings.h>.
#pragma once
#include <stddef.h>

extern "C" {
int bcmp(const void* a, const void* b, size_t n);
void bcopy(const void* src, void* dst, size_t n);
void bzero(void* s, size_t n);
int strcasecmp(const char* a, const char* b);
int strncasecmp(const char* a, const char* b, size_t n);
}
