/* StartCactus: the start function of the cactus track object (MAT3 targets.c _StartCactus). A target's start
 * function runs when a car first hits it (CheckTargetCollision through targetInfo[].start); the cactus then plays
 * its knock-over animation, an environment visual that swaps through its models (AnimateObjSwapGeneric). */
#include "game/game.h"


/* The car record's view (MAT3 tCarData): only its velocity, in the PC layout. */
typedef struct GameCar {
    u8 pad000[0x14];
    f32 dr_vel[3];          /* 0x14 the velocity, feet a second */
    u8 pad020[0x110 - 0x20]; struct pc_visual visuals[21]; u8 pad308[0x380 - 0x308]; void *input; u8 pad384[0x3B8 - 0x384];
} GameCar;


/* Start the cactus a car has just hit (the target t): an environment visual (the next free one, if any) runs its
 * animation for 7 frames, the cactus turns to fall the way the car was going, and the cactus's start sound plays. */
void StartCactus(tTarget *t)
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
        v->data = (uintptr_t) t;
        v->timeStamp = cactus_frame_time;
        car = PC_CAR(t->slot);
        fallDir[0] = car->dr_vel[0] * 0.125f;
        fallDir[1] = car->dr_vel[1] * 0.125f;
        fallDir[2] = car->dr_vel[2] * 0.125f;
        LookInDir(&fallDir[0], t->mat.uvs);
        t->Flags = (u8) (t->Flags & ~(TARGET_ACTIVE | TARGET_HIT));
        v->next = gEnvList;
        gEnvList = (struct tVisual *) v;
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
    }
}
