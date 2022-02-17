#ifndef UTIL_HEADER_H_
#define UTIL_HEADER_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static inline int fpeekc(FILE* f) { int tmp = fgetc(f); ungetc(tmp, f); return tmp; }

static inline char* AllocStringCopy(const char* source)
{
    char* m = malloc(strlen(source));
    strcpy(m, source);
    return m;
}

static inline bool StringEqual(const char* a, const char* b)
{ return strcmp(a, b) == 0; }

static inline bool StringStartsWith(const char* src, const char* sub)
{ return strncmp(src, sub, strlen(sub)) == 0; }

#endif//UTIL_HEADER_H_
