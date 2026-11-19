/* StartWindow: the start function of the breakable windows (MAT3 targets.c _StartWindow). It runs when a car first
 * hits a window (CheckTargetCollision through targetInfo[].start); the window then plays its shattering animation,
 * an environment visual that swaps through its models (AnimateObjSwapGeneric). */
#include "game/game.h"

/* A car has hit the window (the target t; mat.uvs[2] is the way it faces): its animation gets 7 frames, and if an
 * environment visual is free it plays; the window turns round first if the car came from behind it (so the glass
 * flies the car's way), stops colliding, and its start sound plays. */
void StartWindow(tTarget *t)
{
    struct pc_visual *v;
    tCarData *car;
    TargInfo *info;
    f32 dot;

    info = &targetInfo[t->infoIdx];
    t->frames = 7;
    if (!(v = GrabEnvEntry())) {
        return;
    }
    v->index = 0;
    v->func = info->visFunc;
    v->data = (uintptr_t) t;
    v->timeStamp = 0.0333333f;
    car = &pc_cars[t->slot];
    dot = car->dr_vel[0] * t->mat.uvs[2][0] + car->dr_vel[1] * t->mat.uvs[2][1] + car->dr_vel[2] * t->mat.uvs[2][2];
    if (dot > 0.0f) {
        YawUV(3.1415927f, t->mat.uvs);
    }
    t->Flags &= ~(TARGET_ACTIVE | TARGET_HIT);
    v->next = gEnvList;
    gEnvList = (tVisual *) v;
    PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
}
