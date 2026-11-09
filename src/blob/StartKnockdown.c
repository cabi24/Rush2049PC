/* StartKnockdown: the start function of the signs and meters a car knocks over (MAT3 targets.c _StartKnockdown).
 * It runs when a car first hits one (CheckTargetCollision through targetInfo[].start) and sets up the fall that
 * AnimateKnockdown plays. */
#include "game/game.h"



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

/* A car has hit the sign (the target t): if an environment visual is free, the sign turns to face the way the car
 * was going and falls over for 4 frames under AnimateKnockdown (MAT3 8), stops colliding, and its start sound
 * plays. */
void StartKnockdown(tTarget *t)
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
    t->Flags &= ~(TARGET_ACTIVE | TARGET_HIT);
    car = PC_CAR(t->slot);
    PointInDir(car->dr_vel, &facing);
    CopyUV(facing.uvs, t->mat.uvs);
    v->next = gEnvList;
    gEnvList = (struct tVisual *) v;
    PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
}
