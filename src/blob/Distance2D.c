#include "game/game.h"

f32 sqrtf(f32);
#pragma intrinsic (sqrtf)
f32 Distance2D(f32 x,f32 y) {return sqrtf(x*x+y*y);}
