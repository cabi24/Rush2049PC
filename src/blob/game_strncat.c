/* game_strncat: the game's strncat (the N64's own C library routines). */
#include "game/game.h"

/* Appends at most n bytes of s2 to s1, always ending it with a zero, and returns s1. */
char *game_strncat(char *s1, const char *s2, unsigned int n) {
    char *r = s1;
    while (*s1 != 0) s1++;
    while (n-- != 0 && (*s1++ = *s2++) != 0) {
        if (n == 0) *s1 = 0;
    }
    return r;
}
