#include "game/game.h"

typedef struct GameCar {
    u8 pad0[8];
    f32 RWR[3];             /* 0x08 */
    u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x380 - 0x308]; void *input; u8 pad14_384[0x3B8 - 0x384];
} GameCar;                  /* 0x3B8 */

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

s32 TargetCollCylinder(s16 *slotp, f32 *pos, f32 *radiusp, f32 *dist)
{
    f32 dsq, gap;
    f32 vec[3];
    f32 p[3];
    s16 slot = *slotp;
    f32 radius = *radiusp;
    GameCar *gc;

    if (PC_MODEL_COLLIDABLE(slot) == 0) {
        return 0;
    }
    p[0] = pos[0];
    p[1] = pos[1];
    p[2] = pos[2];
    gc = PC_CARS;
    gc += slot;
    vec[0] = gc->RWR[0] - p[0];
    vec[1] = gc->RWR[1] - p[1];
    vec[2] = gc->RWR[2] - p[2];
    dsq = vec[0] * vec[0] + vec[2] * vec[2];
    radius += 3.5f;
    gap = dsq - radius * radius;
    if (dist) {
        *dist = gap;
    }
    if (gap > 0.0f) {
        return 0;
    }
    if (vec[1] > -2.0f && vec[1] < 18.0f) {
        return 1;
    }
    return 0;
}
