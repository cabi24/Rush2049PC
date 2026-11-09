#include "game/game.h"

s32 seeded_rand(u32 *seed) {
    *seed = *seed * 1103515245 + 12345;
    return (*seed >> 16) & 0x7fff;
}
