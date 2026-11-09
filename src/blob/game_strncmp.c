/* game_strncmp.c: compare two strings up to a length (the game's copy of the C library's strncmp). */
#include "game/game.h"

/* strncmp: compare at most n bytes of s1 and s2 (unsigned), stopping at the end of either; 0 for n == 0. */
int game_strncmp(const unsigned char *s1, const unsigned char *s2, unsigned int n) {
    if (n == 0) return 0;
    while (n-- != 0 && *s1 == *s2) {
        if (n == 0 || *s1 == 0 || *s2 == 0) break;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}
