/* game_memmove: the game's memmove (the N64's own C library routines). */
#include "game/game.h"

/* Copies n bytes from src to dst, the regions allowed to overlap: backwards when dst lies inside src, else forwards;
 * four bytes at a time while both are 4-byte aligned. Returns dst. */
void *game_memmove(void *dst, const void *src, u32 n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    if (s < d && d < s + n) {
        d += n;
        s += n;
        if (((uintptr_t)d & 3) == 0 && ((uintptr_t)s & 3) == 0) {
            while (n >= 4) {
                *(u32 *)(d -= 4) = *(const u32 *)(s - 4);
                n -= 4; s -= 4;
            }
        }
        while (n--) { *--d = *--s; }
    } else {
        if (((uintptr_t)d & 3) == 0 && ((uintptr_t)s & 3) == 0) {
            while (n >= 4) {
                *(u32 *)d = *(const u32 *)s;
                d += 4; s += 4; n -= 4;
            }
        }
        while (n--) { *d++ = *s++; }
    }
    return dst;
}
