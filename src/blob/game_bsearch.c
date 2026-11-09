#include "game/game.h"

void *game_bsearch(void *key, void *base, u32 n, u32 size, s32 (*compar)(void *, void *)) {
    s32 adjust;
    s32 unused;
    char *end;
    char *mid;
    s32 result;

    if (n == 0 || size == 0) {
        return 0;
    }
    end = (char *) base + n * size;
    while (n != 0) {
        mid = (char *) base + (n / 2) * size;
        result = compar(key, (void *) mid);
        if (result < 0) {
            n = n / 2;
        } else if (result > 0) {
            base = (void *) (mid + size);
            adjust = (n & 1) ? 0 : 1;
            n = n / 2 - adjust;
        } else {
            return (void *) mid;
        }
    }
    if ((char *) base < end && compar(key, (void *) base) == 0) {
        return (void *) base;
    }
    return 0;
}
