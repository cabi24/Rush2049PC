#include "game/game.h"

float sqrtf(float);
#pragma intrinsic (sqrtf)

VIEW(u8[][0x98], gCamera);
#define gCamera gCamera_view

f32 vector_magnitude(f32 *v) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
#define car ((u8 *)arg0)
#define CF(off) (*(f32 *)(car + (off)))
#define CB(off) (*(s8 *)(car + (off)))
#define OF(off) (*(f32 *)(gCamera[idx] + (off)))
#define A1(off) (*(f32 *)((u8 *)arg1 + (off)))

