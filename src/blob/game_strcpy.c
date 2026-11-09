#include "game/game.h"

u8 *game_strcpy(u8 *arg0, u8 *arg1) {
    u8 *r = arg0;
    while ((*arg0++ = *arg1++) != 0) {
    }
    return r;
}
