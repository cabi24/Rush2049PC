/* InitCarPath: a car's place on the drone paths at the start of a race (MAT3 maxpath.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData; the PC's pc_model): the fields this reads and sets. */
struct tModelData {
    char pad0[0x7CC];
    s8 drone_type; /* 0x7CC 1 a drone, 2 a human */
    char pad7CD[0xF];
    s8 started;    /* 0x7DC it has passed the start checkpoint (laps count from then) */
};

/* The car record (MAT3 tCarData; the PC's pc_car, host layout: its visuals are 0x20 bytes): the fields this sets.
 * Its maxpath control (mpctl, MAT3 tPathPoint) is the PC's pc_car.nav. */
struct tCarData {
    char pad0[0xFA];
    s16 lastResPath; /* 0xFA the alternative path of its last respawn point, -1 the main path */
    s16 lastResIdx;  /* 0xFC the point of that path */
    s16 resIdx;      /* 0xFE */
    char pad100[0x110 - 0x100]; struct pc_visual visuals[21]; char pad100_308[0x314 - 0x308];
    f32 xrel;          /* 0x314 mpctl.xrel */
    f32 yrel;          /* 0x318 mpctl.yrel */
    char pad31C[0x18];
    f32 interval_time; /* 0x334 mpctl.interval_time */
    s16 idx;           /* 0x338 mpctl.idx: the point of its drone path */
    s16 newIdx;        /* 0x33A mpctl.newIdx: a point to jump to, -1 none */
    s16 path;          /* 0x33C mpctl.path: the drone path it follows */
    s16 basePath;      /* 0x33E mpctl.basePath: the path it goes back to after a switch */
};

/* Puts car `gc` (model `m`) at the start of its drone path: the drones take the four drone paths in turn
 * (`next_path` keeps counting from race to race), humans path 0. On a track whose start checkpoint is checkpoint 0
 * the car counts as started already. */
void InitCarPath(struct tModelData *m, struct tCarData *gc) {
    static s32 next_path = 0;

    gc->idx = 0;
    gc->xrel = 0.0f;
    gc->yrel = 0.0f;
    gc->interval_time = gModelTime;
    gc->newIdx = -1;
    if (m->drone_type == 1) {
        gc->path = next_path;
        gc->basePath = next_path;
        next_path++;
        if (next_path >= 4) {
            next_path = 0;
        }
    } else {
        gc->path = 0;
    }
    if (gCheckpoints.startPt == 0) {
        m->started = 1;
    }
    gc->lastResPath = -1;
    gc->lastResIdx = 0;
    gc->resIdx = 0;
}
