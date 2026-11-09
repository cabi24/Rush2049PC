/* TargetCollTrigger: collision test 5 of the track objects (gTargetCollFuncPtrs[5], MAT3 targets.c
 * _TargetCollTrigger), for the path targets that a car sets off by driving onto marked road (a drawbridge, a
 * tram that starts when a car comes by). It keeps the trigger's "cars on it" bits up to date and asks the target to
 * start; it never reports a hit itself. */
#include "game/game.h"

/* The model record's view (MAT3 tModelData, 0x808 bytes, kept in its N64 layout): what the test reads. */
typedef struct Car {
    u8 pad0[0x5A0];
    u16 wheelPoly[4];       /* 0x5A0 tiresli: the road polygon under each tyre */
    u8 pad5A8[0x640 - 0x5A8];
    s8 crashflag;           /* 0x640 the car is wrecked */
    u8 pad641[0x6C4 - 0x641];
    s16 moving_state;       /* 0x6C4 resurrect.moving_state: not negative while it is respawning */
    u8 pad6C6[0x808 - 0x6C6];
} Car;

/* PC: the target (a level-object node), its path record (MAT3 tTargetStructPath, a heap record of its category)
 * and the path (MAT3 tBasicTargetPath, a track WAD record) keep their N64 layout: their pointers are 32-bit window
 * addresses (docs/port/POINTERS.md 10). */
enum n64p_PathData { n64p_PathData_ };
enum n64p_TargetPath { n64p_TargetPath_ };
/* The path (MAT3 tBasicTargetPath): only its flags and its trigger number. */
typedef struct TargetPath {
    u8 pad00[0x10];
    u32 flags;              /* 0x10 0x200 start, 0x4000 restart while cars are on it; bits 24-31 the cars on it */
    s16 count;              /* 0x14 */
    s16 id;                 /* 0x16 collIdx: the trigger number its polygons carry */
} TargetPath;
/* The target's path record (MAT3 tTargetStructPath): first the path. */
typedef struct PathData {
    enum n64p_TargetPath path;
} PathData;
/* A track object (MAT3 tTarget, 0x70 bytes): only its per-kind data, the path record. */
typedef struct Target {
    u8 pad00[0x6C];
    enum n64p_PathData data; /* 0x6C its path record (a window address) */
} Target;
#define N64P(x) _Generic((x), enum n64p_PathData: (PathData *)PC_N64P(x), enum n64p_TargetPath: (TargetPath *)PC_N64P(x), default: (x))

#define TRIGGER_POLY 0x20       /* a polygon's flag: part of a trigger */
#define TRIGGER_NUMBER 0xF800   /* a polygon's trigger number, << 11 */
#define PATH_START 0x200        /* the path target starts moving */
#define PATH_RETRIGGER 0x4000   /* a car on the trigger starts it again even when others are on it */

/* PC: the cars on a trigger are bits 24-31 of its flags (car n: bit 24 + n), room for 8. Cars 8 and up (the more-cars
 * setting) keep theirs in their car record's `scenes` word, a bit per trigger number below 32 (pc_game_types.h).
 * scene_cars_past_8 says whether any of those is on this trigger; scene_car puts car idx on it (in) or off it. */
static u32 scene_cars_past_8(TargetPath *path)
{
    s32 j;
    u32 any = 0;

    if ((u32)path->id < 32) {
        for (j = 8; j < gNumCars; j++) {
            any |= (PC_CAR_SCENES(j) >> path->id) & 1;
        }
    }
    return any;
}
static void scene_car(TargetPath *path, s32 idx, s32 in)
{
    u32 bit;

    if (idx < 8) {
        bit = 1u << (idx + 24);
        path->flags = in ? path->flags | bit : path->flags & ~bit;
    } else if ((u32)path->id < 32) {
        bit = 1u << path->id;
        PC_CAR_SCENES(idx) = in ? PC_CAR_SCENES(idx) | bit : PC_CAR_SCENES(idx) & ~bit;
    }
}
#define SCENE_CARS(path) ((path->flags >> 24) | scene_cars_past_8(path))
#define SCENE_CAR_IN(path, idx) scene_car(path, idx, 1)
#define SCENE_CAR_OUT(path, idx) scene_car(path, idx, 0)

/* Car *player against the trigger of target t: a car that is wrecked or respawning is ignored. If any of its
 * four tyres is on a polygon of this trigger, the path starts when it is the first car on (or always, with
 * PATH_RETRIGGER) and the car is marked on it; else the car is marked off it. Always returns 0. */
s32 TargetCollTrigger(Target *t, s16 *player, s32 unused3, s32 unused4) {
    s32 idx;
    Car *car;
    PathData *pathData;
    TargetPath *path;
    s32 id;
    u16 f0, f1, f2, f3;

    idx = *player;
    car = PC_MODEL(idx);
    if (car->crashflag != 0 || car->moving_state >= 0) {
        return 0;
    }
    pathData = N64P(t->data);
    path = N64P(pathData->path);
    id = path->id;
    id <<= 11;
    f0 = stree_surf[car->wheelPoly[0]].snd_flags;
    f1 = stree_surf[car->wheelPoly[1]].snd_flags;
    f2 = stree_surf[car->wheelPoly[2]].snd_flags;
    f3 = stree_surf[car->wheelPoly[3]].snd_flags;
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
