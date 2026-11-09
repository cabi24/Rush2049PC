#include "game/game.h"

s32 objcmp(u8 *arg0, u8 *arg1) {
    if ((arg0 == NULL) || (arg1 == NULL)) {
        return -1;
    }
    return game_strncmp(arg0, arg1, 0xF);
}
