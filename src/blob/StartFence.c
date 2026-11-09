/* StartFence: the start function of the breakable fence track object (MAT3 targets.c _StartFence). It runs when a
 * car first hits the fence (CheckTargetCollision through targetInfo[].start); the fence then plays its breaking
 * animation, an environment visual that swaps through its models (AnimateObjSwapGeneric). */
#include "game/game.h"

/* Start the fence a car has just hit (the target t): an environment visual (the next free one, if any) runs its
 * 12-frame animation, the fence stops colliding and its start sound plays. */
void StartFence(tTarget *t)
{
    struct pc_visual *v;
    TargInfo *info;

    info = &targetInfo[t->infoIdx];
    t->frames = 12;
    v = GrabEnvEntry();
    if (v != NULL) {
        v->index = 0;
        v->func = info->visFunc;
        v->data = (uintptr_t) t;
        v->timeStamp = fence_frame_time;
        t->Flags = (u8) (t->Flags & ~(TARGET_ACTIVE | TARGET_HIT));
        v->next = gEnvList;
        gEnvList = (struct tVisual *) v;
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
    }
}
