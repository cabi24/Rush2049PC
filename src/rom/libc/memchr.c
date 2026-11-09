/* The C library's memchr, as the game links its own. */
#include "static_debug_context.h"

/* The first byte equal to c among the n at s, or 0. */
u8 *memchr(u8 *src, u32 c, s32 count) { u8 *p=src; while(count--) { if(*p==c) return p; p++; } return NULL; }

