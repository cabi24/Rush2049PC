/* StartCone: the start function of the cones and gas pumps a car can send flying (MAT3 targets.c _StartCone). It
 * runs when a car first hits one (CheckTargetCollision through targetInfo[].start); the debris then flies off under
 * AnimateVelRotGeneric. Two cheats change what a cone does: with gCarMines the car picks it up as a mine, with
 * gMineCones it blows up under the car. */
#include "game/game.h"



#define OBJ_CONE 236            /* the cone's object (MAT3 550) */
#define OBJ_LIGHT_DEBRIS_A 339  /* the two objects that fly up only a foot a frame (MAT3 653 and 551) */
#define OBJ_LIGHT_DEBRIS_B 237

/* Wreck the model's car, unless it already is (MAT3: `if (!model[slot].crashflag) model[slot].crashflag = 1`). */
static void crash_car(tModelData *m)
{
    if (!m->crashflag) {
        m->crashflag = 1;
    }
}

/* A car has hit the cone or pump. With the gCarMines cheat a cone is taken as the car's mine; with gMineCones it
 * blows up and wrecks the car. Otherwise an environment visual (if one is free) flies it off for 5 seconds: an
 * eighth of the car's speed plus a hop up (1 ft a frame for the two light objects, else 2), a fixed spin, facing
 * the way the car went. The target stops colliding either way, and the start sound plays when it flies. t is the
 * target: its type the object it shows, its data its tTargetStructVelRot. */
void StartCone(tTarget *t)
{
    struct pc_visual *v;
    f32 tmat[4][3];
    f32 *vel;
    tTargetStructVelRot *motion;
    tCarData *car;
    TargInfo *info;

    info = &targetInfo[t->infoIdx];
    car = &pc_cars[t->slot];
    if (gCarMines && info->objListIdx == OBJ_CONE) {
        car->carMine = 1;
        MBOX_HideObject(t->dlIdx, 0, 15);
        t->Flags &= ~(TARGET_ACTIVE | TARGET_HIT);
        return;
    }
    if (gMineCones && info->objListIdx == OBJ_CONE) {
        MBOX_HideObject(t->dlIdx, 0, 15);
        StartBlast(t->pos, 0, 0.4f, 1);
        crash_car(&pc_models[t->slot]);
        t->Flags &= ~(TARGET_ACTIVE | TARGET_HIT);
        return;
    }
    v = GrabEnvEntry();
    if (v == 0) {
        return;
    }
    v->index = 0;
    v->func = info->visFunc;
    v->data = (uintptr_t) t;
    v->timeStamp = 5.0f;           /* the seconds the flight lasts */
    t->Flags &= ~(TARGET_ACTIVE | TARGET_HIT);
    vel = ((tTargetStructVelRot *) t->data)->vel;
    motion = t->data;
    vel[0] = car->dr_vel[0] * 0.125f;
    vel[1] = car->dr_vel[1] * 0.125f;
    vel[2] = car->dr_vel[2] * 0.125f;
    LookInDir(vel, tmat);
    if (t->type == OBJ_LIGHT_DEBRIS_A || t->type == OBJ_LIGHT_DEBRIS_B) {
        vel[1] += 1.0f;
        motion->angV[0] = 0.0f;
        motion->angV[1] = 12.0f;
        motion->angV[2] = 15.0f;
    } else {
        vel[1] += 2.0f;
        motion->angV[0] = 0.0f;
        motion->angV[1] = 12.0f;
        motion->angV[2] = 15.0f;
    }
    CopyUV(tmat, t->mat.uvs);
    v->next = gEnvList;
    gEnvList = (struct tVisual *) v;
    PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
}
