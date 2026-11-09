/* StartCactus: the start function of the cactus track object (MAT3 targets.c _StartCactus). A target's start
 * function runs when a car first hits it (CheckTargetCollision through targetInfo[].start); the cactus then plays
 * its knock-over animation, an environment visual that swaps through its models (AnimateObjSwapGeneric). */
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
    u8 pad44[0x5A - 0x44];
    s16 frames;             /* 0x5A the animation's frame count */
    s8 slot;                /* 0x5C the car that hit it */
} Target;

/* The car record's view (MAT3 tCarData): only its velocity, in the PC layout. */
typedef struct GameCar {
    u8 pad000[0x14];
    f32 dr_vel[3];          /* 0x14 the velocity, feet a second */
    u8 pad020[0x110 - 0x20]; struct pc_visual visuals[21]; u8 pad308[0x380 - 0x308]; void *input; u8 pad384[0x3B8 - 0x384];
} GameCar;


/* Start the cactus a car has just hit: an environment visual (the next free one, if any) runs its animation for 7
 * frames, the cactus turns to fall the way the car was going, and the cactus's start sound plays. */
void StartCactus(Target *t)
{
    struct pc_visual *v;
    TargInfo *info;
    GameCar *car;
    f32 fallDir[3];

    info = &targetInfo[t->infoIdx];
    t->frames = 7;
    v = GrabEnvEntry();
    if (v != NULL) {
        v->index = 0;
        v->func = info->visFunc;
        v->data = PC_N64W(t);
        v->timeStamp = cactus_frame_time;
        car = PC_CAR(t->slot);
        fallDir[0] = car->dr_vel[0] * 0.125f;
        fallDir[1] = car->dr_vel[1] * 0.125f;
        fallDir[2] = car->dr_vel[2] * 0.125f;
        LookInDir(&fallDir[0], t->uvs);
        t->flags = (u8) (t->flags & ~(TARGET_ACTIVE | TARGET_HIT));
        v->next = gEnvList;
        gEnvList = (struct Node *) v;
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->matPos, 2);
    }
}
