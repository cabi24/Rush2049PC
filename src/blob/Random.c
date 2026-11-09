#include "game/game.h"

f32 Random(f32 max)
{
    f32 rannum;

    rannum = ((f32)(game_rand() & 0x07FFF) * max) / 32768.0f;
    return rannum;
}
