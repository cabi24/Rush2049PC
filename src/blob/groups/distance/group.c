#include "game/game.h"

f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

f32 Distance(f32 x, f32 y, f32 z)
{
    return sqrtf((x * x + y * y) + z * z);
}
