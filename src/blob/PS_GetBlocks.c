#include "game/game.h"

s32 PS_GetBlocks(s32 arg0) {
    return (s32) (arg0 + 0xFF) >> 8;
}
