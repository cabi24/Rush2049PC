#include "game/game.h"

u8 *game_strcat(u8 *arg0, u8 *arg1) {
    u8 *r = arg0;
    while (*arg0 != 0) {
        arg0++;
    }
    while ((*arg0++ = *arg1++) != 0) {
    }
    return r;
}
