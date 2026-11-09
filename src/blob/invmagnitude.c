#include "game/game.h"

extern f32 sqrtf(f32);
#pragma intrinsic (sqrtf)
f32 invmagnitude(f32 *v) {
 f32 sum=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
 if(sum<invmagnitude_floor) sum=invmagnitude_floor;
 return 1.0f/sqrtf(sum);
}
