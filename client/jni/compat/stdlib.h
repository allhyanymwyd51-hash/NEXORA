// NEXORA engine — freestanding <stdlib.h>.
#pragma once
#include <stddef.h>

extern "C" {
void* malloc(size_t n);
void* calloc(size_t n, size_t size);
void* realloc(void* p, size_t n);
void free(void* p);
void* aligned_alloc(size_t align, size_t size);
int posix_memalign(void** out, size_t align, size_t size);
int atoi(const char* s);
long atol(const char* s);
long long atoll(const char* s);
double atof(const char* s);
float strtof(const char* s, char** end);
double strtod(const char* s, char** end);
long strtol(const char* s, char** end, int base);
unsigned long strtoul(const char* s, char** end, int base);
unsigned long long strtoull(const char* s, char** end, int base);
void qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));
char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
int abs(int v);
long labs(long v);
long long llabs(long long v);
int rand(void);
void srand(unsigned int seed);
void exit(int code);
void abort(void);
char* realpath(const char* path, char* resolved);
int system(const char* cmd);
}
