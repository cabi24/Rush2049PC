/* StartFence: the start function of the breakable fence track object (MAT3 targets.c _StartFence). It runs when a
 * car first hits the fence (CheckTargetCollision through targetInfo[].start); the fence then plays its breaking
 * animation, an environment visual that swaps through its models (AnimateObjSwapGeneric). */
#include "game/game.h"

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_ACTIVE, TARGET_HIT */
    u8 pad05[11];
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[0x38 - 0x12];
    f32 matPos[3];          /* 0x38 its position, where its sounds play */
    u8 pad44[0x5A - 0x44];
    s16 frames;             /* 0x5A the animation's frame count */
    s8 slot;                /* 0x5C the car that hit it */
} Target;


/* Start the fence a car has just hit: an environment visual (the next free one, if any) runs its 12-frame
 * animation, the fence stops colliding and its start sound plays. */
void StartFence(Target *t)
{
    struct pc_visual *v;
    TargInfo *info;

    info = &targetInfo[t->infoIdx];
    t->frames = 12;
    v = GrabEnvEntry();
    if (v != NULL) {
        v->index = 0;
        v->func = info->visFunc;
        v->data = PC_N64W(t);
        v->timeStamp = fence_frame_time;
        t->flags = (u8) (t->flags & ~(TARGET_ACTIVE | TARGET_HIT));
        v->next = gEnvList;
        gEnvList = (struct Node *) v;
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->matPos, 2);
    }
}
