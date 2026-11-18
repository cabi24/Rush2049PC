/* TargetCollCylinder: collision test 1 of the track objects (gTargetCollFuncPtrs[1], MAT3 targets.c
 * _TargetCollCylinder): a car against an upright cylinder (in MAT3's table, the battle weapon icons). */
#include "game/game.h"

/* Does car *slotp touch the cylinder of radius *radiusp standing at pos? The car counts as a 3.5 ft circle; it
 * hits when the circles overlap and the car is between 2 ft below the cylinder's base and 18 ft above it. dist, if
 * not NULL, gets the squared distance minus the squared radius (not positive: overlapping). A car that isn't
 * collidable never hits. */
s32 TargetCollCylinder(s16 *slotp, f32 *pos, f32 *radiusp, f32 *dist)
{
    f32 dsq, gap;
    f32 vec[3];
    f32 p[3];
    s16 slot = *slotp;
    f32 radius = *radiusp;
    tCarData *gc;

    if (PC_MODEL_COLLIDABLE(slot) == 0) {
        return 0;
    }
    p[0] = pos[0];
    p[1] = pos[1];
    p[2] = pos[2];
    gc = pc_cars;
    gc += slot;
    vec[0] = gc->dr_pos[0] - p[0];
    vec[1] = gc->dr_pos[1] - p[1];
    vec[2] = gc->dr_pos[2] - p[2];
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
