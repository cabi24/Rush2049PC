/* game_strcpy: the game's strcpy (the N64's own C library routines). */
#include "game/game.h"

/* Copies src, its zero included, to dst and returns dst. */
u8 *game_strcpy(u8 *dst, u8 *src) {
    u8 *r = dst;
    while ((*dst++ = *src++) != 0) {
    }
    return r;
}
