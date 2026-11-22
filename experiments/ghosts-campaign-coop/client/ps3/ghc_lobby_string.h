#pragma once
#include <stddef.h>

static inline size_t ghc_strlen(const char *value) {
    size_t length = 0;
    if (!value) return 0;
    while (value[length]) ++length;
    return length;
}

static inline void ghc_memcpy(void *destination, const void *source, size_t length) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    size_t i;
    for (i = 0; i < length; ++i) out[i] = in[i];
}

static inline void ghc_memset(void *destination, int value, size_t length) {
    unsigned char *out = (unsigned char *)destination;
    size_t i;
    for (i = 0; i < length; ++i) out[i] = (unsigned char)value;
}

static inline int ghc_memcmp(const void *left, const void *right, size_t length) {
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    size_t i;
    for (i = 0; i < length; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

static inline int ghc_strncmp(const char *left, const char *right, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) {
        unsigned char a = (unsigned char)left[i];
        unsigned char b = (unsigned char)right[i];
        if (a != b) return a < b ? -1 : 1;
        if (a == 0) return 0;
    }
    return 0;
}
