/* InitCarPath: a car's place on the drone paths at the start of a race (MAT3 maxpath.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData; the PC's pc_model): the fields this reads and sets. */
struct tModelData {
    _tCar *car;
    Tune44 *tune;
    char pad8[0x398];
    streetyp *lasttp[4];
    char pad3B0[0x5C];
    const s16 *torquecurve;
    char pad410[0x238];
    surftyp *movingSurface;
    char pad64C[0x180];
    s8 drone_type; /* 0x7CC 1 a drone, 2 a human */
    char pad7CD[0xF];
    s8 started;    /* 0x7DC it has passed the start checkpoint (laps count from then) */
};

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
