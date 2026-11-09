/* The C library's strchr, strlen and memcpy, as the game links its own (libultra's string.c). */
#include "../rom_tu.h"

/* The first c in the string s, or 0. */
char *strchr(const char *s, int c) {
    const char ch = c;

    while (*s != ch) {
        if (*s == 0) {
            return NULL;
        }
        s++;
    }
    return (char *)s;
}

/* The length of the string s. */
u32 strlen(const char *str) {
    const char *p;

    p = str;
    if (*(u8 *)p != 0) {
        do {
            p++;
        } while (*(u8 *)p != 0);
    }
    return (u32)(p - str);
}

/* Copies n bytes from s2 to s1; returns s1. */
void *memcpy(void *s1, const void *s2, u32 n) {
    char *su1 = (char *)s1;
    const char *su2 = (const char *)s2;

    while (n > 0) {
        *su1 = *su2;
        su1++;
        su2++;
        n--;
    }
    return (void *)s1;
}

