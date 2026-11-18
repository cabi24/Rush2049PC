/* TargetCollTrigger: collision test 5 of the track objects (gTargetCollFuncPtrs[5], MAT3 targets.c
 * _TargetCollTrigger), for the path targets that a car sets off by driving onto marked road (a drawbridge, a
 * tram that starts when a car comes by). It keeps the trigger's "cars on it" bits up to date and asks the target to
 * start; it never reports a hit itself. */
#include "game/game.h"

/* The target's data is its path record (MAT3 tTargetStructPath), which names its path (TARGET_PATH: a track WAD
 * record, tBasicTargetPath). What this reads of the path: its flags (PATH_START, PATH_RETRIGGER; bits 24-31 the cars
 * on it) and its collision object's number (collIdx), which is the trigger number its polygons carry. */
#define TRIGGER_POLY 0x20       /* a polygon's flag: part of a trigger */
#define TRIGGER_NUMBER 0xF800   /* a polygon's trigger number, << 11 */
#define PATH_START 0x200        /* the path target starts moving */
#define PATH_RETRIGGER 0x4000   /* a car on the trigger starts it again even when others are on it */

/* PC: the cars on a trigger are bits 24-31 of its flags (car n: bit 24 + n), room for 8. Cars 8 and up (the more-cars
 * setting) keep theirs in their car record's `scenes` word, a bit per trigger number below 32 (pc_game_types.h).
 * scene_cars_past_8 says whether any of those is on this trigger; scene_car puts car idx on it (in) or off it. */
static u32 scene_cars_past_8(tBasicTargetPath *path)
{
    s32 j;
    u32 any = 0;

    if ((u32)path->collIdx < 32) {
        for (j = 8; j < gNumCars; j++) {
            any |= (pc_cars[j].scenes >> path->collIdx) & 1;
        }
    }
    return any;
}
static void scene_car(tBasicTargetPath *path, s32 idx, s32 in)
{
    u32 bit;

    if (idx < 8) {
        bit = 1u << (idx + 24);
        path->flags = in ? path->flags | bit : path->flags & ~bit;
    } else if ((u32)path->collIdx < 32) {
        bit = 1u << path->collIdx;
        pc_cars[idx].scenes = in ? pc_cars[idx].scenes | bit : pc_cars[idx].scenes & ~bit;
    }
}
#define SCENE_CARS(path) ((path->flags >> 24) | scene_cars_past_8(path))
#define SCENE_CAR_IN(path, idx) scene_car(path, idx, 1)
#define SCENE_CAR_OUT(path, idx) scene_car(path, idx, 0)

/* Car *player against the trigger of target t (its data is its path record): a car that is wrecked or respawning is
 * ignored. If any of its four tyres is on a polygon of this trigger, the path starts when it is the first car on (or
 * always, with PATH_RETRIGGER) and the car is marked on it; else the car is marked off it. Always returns 0. */
s32 TargetCollTrigger(tTarget *t, s16 *player, s32 unused3, s32 unused4) {
    s32 idx;
    tModelData *car;
    tTargetStructPath *state;
    tBasicTargetPath *path;
    s32 id;
    u16 f0, f1, f2, f3;

    idx = *player;
    car = &pc_models[idx];
    if (car->crashflag != 0 || car->resurrect.moving_state >= 0) {
        return 0;
    }
    state = t->data;
    path = TARGET_PATH(state);
    id = path->collIdx;
    id <<= 11;
    f0 = stree_surf[car->tiresli[0]].snd_flags;
    f1 = stree_surf[car->tiresli[1]].snd_flags;
    f2 = stree_surf[car->tiresli[2]].snd_flags;
    f3 = stree_surf[car->tiresli[3]].snd_flags;
    if (((f0 & TRIGGER_POLY) && (f0 & TRIGGER_NUMBER) == id) || ((f1 & TRIGGER_POLY) && (f1 & TRIGGER_NUMBER) == id) ||
        ((f2 & TRIGGER_POLY) && (f2 & TRIGGER_NUMBER) == id) || ((f3 & TRIGGER_POLY) && (f3 & TRIGGER_NUMBER) == id)) {
        if (SCENE_CARS(path) == 0 || (path->flags & PATH_RETRIGGER)) {
            path->flags |= PATH_START;
        }
        SCENE_CAR_IN(path, idx);
    } else {
        SCENE_CAR_OUT(path, idx);
    }
    return 0;
}
