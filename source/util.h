#ifndef UTIL_HEADER_H_
#define UTIL_HEADER_H_
#include <stdio.h>

static inline int fpeekc(FILE* f) { int tmp = fgetc(f); ungetc(tmp, f); return tmp; }

static inline char* AllocStringCopy(const char* source)
{
    char* m = malloc(strlen(source));
    strcpy(m, source);
    return m;
}

#endif//UTIL_HEADER_H_
