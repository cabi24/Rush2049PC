#include "game/game.h"

int game_strncmp(const unsigned char *s1, const unsigned char *s2, unsigned int n) {
    if (n == 0) return 0;
    while (n-- != 0 && *s1 == *s2) {
        if (n == 0 || *s1 == 0 || *s2 == 0) break;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

