/* game_strcat: the game's strcat (the N64's own C library routines). */
#include "game/game.h"

/* Appends src to dst and returns dst. */
u8 *game_strcat(u8 *dst, u8 *src) {
    u8 *r = dst;
    while (*dst != 0) {
        dst++;
    }
    while ((*dst++ = *src++) != 0) {
    }
    return r;
}
