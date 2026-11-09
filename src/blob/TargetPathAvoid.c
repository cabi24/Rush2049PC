/* TargetPathAvoid: a drone slows for the moving boxes ahead of it (MAT3 targets.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData): its dead-reckoned position and orientation (reckon.rwr, reckon.uvs). */
struct tModelData {
    char pad0[0x794];
    f32 reckon_rwr[3];
    f32 reckon_uvs[3][3];
};

extern f32 fabsf(f32);

/* Lowers *scale1, the drone's speed scale (AvoidAreas), for each bounding box model ahead of model `m` within 225
 * feet and 14 feet to either side: to 0.75 within 125 feet, rising to 1 at 225. Original game: the loop stops before
 * the last box in use (it runs to gBBoxModelUsed - 1); MAT3 has it so too. */
void TargetPathAvoid(struct tModelData *m, f32 *scale1) {
    tMiniModelData *tp;
    f32 pos[3];
    f32 rpos[3];
    f32 scale2;

    if (gBBoxModelUsed != 0) {
        for (tp = gBBoxModelStructs; tp < &gBBoxModelStructs[gBBoxModelUsed - 1]; tp++) {
            pos[0] = tp->pos[0] - m->reckon_rwr[0];
            pos[1] = tp->pos[1] - m->reckon_rwr[1];
            pos[2] = tp->pos[2] - m->reckon_rwr[2];
            BodyVector(pos, rpos, m->reckon_uvs[0]);
            if (rpos[2] < 0.0f) {
                continue;
            }
            if (rpos[2] > 225.0f) {
                continue;
            }
            if (fabsf(rpos[0]) > 14.0f) {
                continue;
            }
            if (rpos[2] < 125.0f) {
                scale2 = 0.75f;
            } else if (rpos[2] < 225.0f) {
                scale2 = 1.0f - (225.0f - rpos[2]) * 0.0025f;
            } else {
                scale2 = 1;
            }
            if (scale2 < *scale1) {
                *scale1 = scale2;
            }
        }
    }
}
