/* InitCarPath: a car's place on the drone paths at the start of a race (MAT3 maxpath.c). */
#include "game/game.h"

/* Puts car `gc` (model `m`) at the start of its drone path: the drones take the four drone paths in turn
 * (`next_path` keeps counting from race to race), humans path 0. On a track whose start checkpoint is checkpoint 0
 * the car counts as started already. */
void InitCarPath(struct tModelData *m, struct tCarData *gc) {
    static s32 next_path = 0;

    gc->mpctl.idx = 0;
    gc->mpctl.xrel = 0.0f;
    gc->mpctl.yrel = 0.0f;
    gc->mpctl.interval_time = gModelTime;
    gc->mpctl.newIdx = -1;
    if (m->drone_type == 1) {
        gc->mpctl.path = next_path;
        gc->mpctl.basePath = next_path;
        next_path++;
        if (next_path >= 4) {
            next_path = 0;
        }
    } else {
        gc->mpctl.path = 0;
    }
    if (gCheckpoints.startPt == 0) {
        m->started = 1;
    }
    gc->lastResPath = -1;
    gc->lastResIdx = 0;
    gc->resIdx = 0;
}
