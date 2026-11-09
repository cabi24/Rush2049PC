/* StartWindow: the start function of the breakable windows (MAT3 targets.c _StartWindow). It runs when a car first
 * hits a window (CheckTargetCollision through targetInfo[].start); the window then plays its shattering animation,
 * an environment visual that swaps through its models (AnimateObjSwapGeneric). */
#include "game/game.h"

/* The environment visual (MAT3 tVisual) in its PC layout: next and func are host pointers, data the target's
 * window address. */
typedef struct Visual {
    struct Visual *next;    /* 0x00 */
    s16 index;              /* 0x08 */
    s16 objnum;             /* 0x0A */
    s16 slot;               /* 0x0C */
    u8 pad0E[2];
    u32 data;               /* 0x10 the target */
    f32 timeStamp;          /* 0x14 */
    void *func;             /* 0x18 the function called each frame */
} Visual;

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_ACTIVE, TARGET_HIT */
    u8 pad05[11];
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[2];
    f32 uvs[3][3];          /* 0x14 mat: its orientation; uvs[2] is the way it faces */
    f32 matPos[3];          /* 0x38 mat: its position, where its sounds play */
    u8 pad44[22];
    s16 frames;             /* 0x5A the animation's frame count */
    s8 slot;                /* 0x5C the car that hit it */
} Target;

/* The car record's view (MAT3 tCarData) in the PC layout: only its velocity. */
typedef struct GameCar {
    u8 pad0[20];
    f32 dr_vel[3];          /* 0x14 the velocity, feet a second */
    u8 pad20[0x110 - 0x20]; struct pc_visual visuals[21]; u8 pad20_308[0x380 - 0x308]; void *input; u8 pad20_384[0x3B8 - 0x384];
} GameCar;                  /* 0x3B8 */

#define N64P(x) ((void *)PC_N64P(x))
#define N64W(x) PC_N64W(x)

VIEW(Visual *, gEnvList);
#define gEnvList gEnvList_view

Visual *GrabEnvEntry(void);

/* A car has hit the window: its animation gets 7 frames, and if an environment visual is free it plays; the window
 * turns round first if the car came from behind it (so the glass flies the car's way), stops colliding, and its
 * start sound plays. */
void StartWindow(Target *t)
{
    Visual *v;
    GameCar *car;
    TargInfo *info;
    f32 dot;

    info = &targetInfo[t->infoIdx];
    t->frames = 7;
    if (!(v = GrabEnvEntry())) {
        return;
    }
    v->index = 0;
    v->func = info->visFunc;
    v->data = N64W(t);
    v->timeStamp = 0.0333333f;
    car = PC_CAR(t->slot);
    dot = car->dr_vel[0] * t->uvs[2][0] + car->dr_vel[1] * t->uvs[2][1] + car->dr_vel[2] * t->uvs[2][2];
    if (dot > 0.0f) {
        YawUV(3.1415927f, t->uvs);
    }
    t->flags &= ~(TARGET_ACTIVE | TARGET_HIT);
    v->next = gEnvList;
    gEnvList = v;
    PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->matPos, 2);
}
