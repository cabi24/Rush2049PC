/* TargetCollMineCylinder: collision test 2 of the track objects (gTargetCollFuncPtrs[2], MAT3 targets.c
 * _TargetCollMineCylinder): a car against a battle mine, an upright cylinder only 4 ft tall. */
#include "game/game.h"

/* The car record's view (MAT3 tCarData) in the PC layout: only its position. */
typedef struct GameCar {
    u8 pad0[8];
    f32 RWR[3];             /* 0x08 dr_pos: the position, feet */
    u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x380 - 0x308]; void *input; u8 pad14_384[0x3B8 - 0x384];
} GameCar;                  /* 0x3B8 */

/* Does car *slotp touch the mine of radius *radiusp lying at pos? As TargetCollCylinder, but the car must be
 * between 2 ft below the mine and 4 ft above it (so a car flying over doesn't set it off). dist, if not NULL, gets
 * the squared distance minus the squared radius. A car that isn't collidable never hits. */
s32 TargetCollMineCylinder(s16 *slotp, f32 *pos, f32 *radiusp, f32 *dist)
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
    if (vec[1] > -2.0f && vec[1] < 4.0f) {
        return 1;
    }
    return 0;
}
