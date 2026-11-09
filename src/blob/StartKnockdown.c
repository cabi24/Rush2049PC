/* StartKnockdown: the start function of the signs and meters a car knocks over (MAT3 targets.c _StartKnockdown).
 * It runs when a car first hits one (CheckTargetCollision through targetInfo[].start) and sets up the fall that
 * AnimateKnockdown plays. */
#include "game/game.h"


/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_ACTIVE, TARGET_HIT */
    u8 pad05[11];
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[2];
    f32 uvs[3][3];          /* 0x14 mat: its orientation */
    f32 matPos[3];          /* 0x38 mat: its position, where its sounds play */
    u8 pad44[22];
    s16 frames;             /* 0x5A the frames of the fall */
    s8 slot;                /* 0x5C the car that hit it */
} Target;

/* The car record's view (MAT3 tCarData) in the PC layout: only its velocity. */
typedef struct GameCar {
    u8 pad0[20];
    f32 dr_vel[3];          /* 0x14 the velocity, feet a second */
    u8 pad20[0x110 - 0x20]; struct pc_visual visuals[21]; u8 pad20_308[0x380 - 0x308]; void *input; u8 pad20_384[0x3B8 - 0x384];
} GameCar;                  /* 0x3B8 */

/* An orientation and a position (MAT3 tUVMatrix). */
typedef struct MATRIX {
    f32 uvs[3][3];
    f32 pos[3];
} MATRIX;

/* Turn the matrix to look along the direction (MAT3 inlines LookInDir on a tUVMatrix here). */
static void PointInDir(f32 *dir, MATRIX *m)
{
    LookInDir(dir, m->uvs);
}

/* A car has hit the sign: if an environment visual is free, the sign turns to face the way the car was going and
 * falls over for 4 frames under AnimateKnockdown (MAT3 8), stops colliding, and its start sound plays. */
void StartKnockdown(Target *t)
{
    struct pc_visual *v;
    MATRIX facing;
    GameCar *car;

    if (!(v = GrabEnvEntry())) {
        return;
    }
    v->index = 0;
    v->func = targetInfo[t->infoIdx].visFunc;
    v->data = (uintptr_t) t;
    v->timeStamp = 0.0333333f;
    t->frames = 4;
    t->flags &= ~(TARGET_ACTIVE | TARGET_HIT);
    car = PC_CAR(t->slot);
    PointInDir(car->dr_vel, &facing);
    CopyUV(facing.uvs, t->uvs);
    v->next = gEnvList;
    gEnvList = (struct tVisual *) v;
    PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->matPos, 2);
}
