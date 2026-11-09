/* GetResPt and raise_ob_res_pt: the obstacle course's respawn high-water mark (MAT3 resurrect.c, GetResPt()).
 *
 * In an obstacle race a crashed car respawns at the furthest point of the resurrection path it has reached
 * (tModelData.obResPt). The path's points come in pairs, each pair marking a gate: reaching the pair's second
 * point, or going past its first along the pair's direction, counts. */
#include "game/game.h"

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the field used here. */
typedef struct tCarData {
    u8 pad0[8];
    f32 dr_pos[3];              /* 0x08 the position, feet */
    u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x380 - 0x308]; void *input; u8 pad14_384[0x3B8 - 0x384];
} tCarData; /* 0x3B8 */

/* Raise car 0's obResPt to the point of the resurrection path nearest it: the pair's first point when it is
 * nearest the second (odd) one; when it is nearest the first (even) one, that point if the car is past it towards
 * the second (the dot product of the pair's direction with the car's offset from the point), else the previous
 * pair's first point. (MAT3 projects each axis on its own; the N64 takes the dot product.) If the path has no
 * points the nearest index is never set: never so on the retail tracks' obstacle courses. */
void GetResPt(void) {
    s32 i;
    s32 close_index;
    f32 x, y, z;
    f32 dx, dy, dz;
    f32 dist;
    f32 c_dist;
    f32 delta1[3];
    f32 delta2[3];
    s16 *p;
    tCarData *car = PC_CARS;

    x = PC_CAR_POS(0)[0];
    y = PC_CAR_POS(0)[1];
    z = PC_CAR_POS(0)[2];
    c_dist = 1e20;
    for (i = 0; i < gResPath.numPts; i++) {
        dx = gResPath.pts[i][0] - x;
        dy = gResPath.pts[i][1] - y;
        dz = gResPath.pts[i][2] - z;
        dist = dx * dx + dy * dy + dz * dz;
        if (dist < c_dist) {
            c_dist = dist;
            close_index = i;
        }
    }
    if (close_index & 1) {
        close_index--;
        raise_ob_res_pt(close_index);
        return;
    }
    /* p[0..2] the pair's first point, p[3..5] its second */
    p = gResPath.pts[close_index];
    delta1[0] = (f32) p[3] - (f32) p[0];
    delta1[1] = (f32) p[4] - (f32) p[1];
    delta1[2] = (f32) p[5] - (f32) p[2];
    delta2[0] = car->dr_pos[0] - p[0];
    delta2[1] = car->dr_pos[1] - p[1];
    delta2[2] = car->dr_pos[2] - p[2];
    dx = delta1[0] * delta2[0] + delta1[1] * delta2[1] + delta1[2] * delta2[2];
    if (dx < 0.0f && close_index > 0) {
        close_index -= 2;
    }
    raise_ob_res_pt(close_index);
}

/* Car 0's obResPt becomes `index` if that is further along. */
void raise_ob_res_pt(s32 index) {
    if (PC_MODEL_OB_RES_PT(0) < index) {
        PC_MODEL_OB_RES_PT(0) = index;
    }
}
