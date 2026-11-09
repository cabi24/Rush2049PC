/* StartCone: the start function of the cones and gas pumps a car can send flying (MAT3 targets.c _StartCone). It
 * runs when a car first hits one (CheckTargetCollision through targetInfo[].start); the debris then flies off under
 * AnimateVelRotGeneric. Two cheats change what a cone does: with gCarMines the car picks it up as a mine, with
 * gMineCones it blows up under the car. */
#include "game/game.h"

/* The debris' motion (MAT3 tTargetStructVelRot), the target's per-kind data. */
typedef struct VelRot {
    f32 angV[3];            /* 0x00 its spin, radians a second about x, y, z */
    f32 vel[3];             /* 0x0C its velocity, feet a frame */
} VelRot;

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
enum n64p_VelRot { n64p_VelRot_ };
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_ACTIVE, TARGET_HIT */
    u8 pad05[7];
    s32 dlIdx;              /* 0x0C its scene object */
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[2];
    f32 uvs[3][3];          /* 0x14 mat: its orientation */
    f32 matPos[3];          /* 0x38 mat: its position, where its sounds play */
    f32 pos[3];             /* 0x44 its position */
    s16 type;               /* 0x50 its object (a gObjList index) */
    u8 pad52[0x5C - 0x52];
    s8 slot;                /* 0x5C the car that hit it */
    u8 pad5D[0x6C - 0x5D];
    enum n64p_VelRot data;  /* 0x6C its VelRot (a window address) */
} Target;

/* The car record's view (MAT3 tCarData) in the PC layout: the velocity and the picked-up-mine flag. */
typedef struct GameCar {
    u8 pad000[0x14];
    f32 dr_vel[3];          /* 0x14 the velocity, feet a second */
    u8 pad020[0x110 - 0x20]; struct pc_visual visuals[21]; u8 pad308[0x35F - 0x308];
    u8 carMine;             /* 0x35F the car carries a cone as a mine (the gCarMines cheat) */
    u8 pad360[0x380 - 0x360]; void *input; u8 pad384[0x3B8 - 0x384];
} GameCar;

/* The model record's view (MAT3 tModelData): only its crash flag. */
typedef struct ModelData {
    u8 pad000[0x640];
    s8 crashflag;           /* 0x640 the car is wrecked */
    u8 pad641[0x808 - 0x641];
} ModelData;


#define N64P(x) _Generic((x), enum n64p_VelRot: (VelRot *)PC_N64P(x), default: (x))

#define OBJ_CONE 236            /* the cone's object (MAT3 550) */
#define OBJ_LIGHT_DEBRIS_A 339  /* the two objects that fly up only a foot a frame (MAT3 653 and 551) */
#define OBJ_LIGHT_DEBRIS_B 237

/* Wreck the model's car, unless it already is (MAT3: `if (!model[slot].crashflag) model[slot].crashflag = 1`). */
static void crash_car(ModelData *m)
{
    if (!m->crashflag) {
        m->crashflag = 1;
    }
}

/* A car has hit the cone or pump. With the gCarMines cheat a cone is taken as the car's mine; with gMineCones it
 * blows up and wrecks the car. Otherwise an environment visual (if one is free) flies it off for 5 seconds: an
 * eighth of the car's speed plus a hop up (1 ft a frame for the two light objects, else 2), a fixed spin, facing
 * the way the car went. The target stops colliding either way, and the start sound plays when it flies. */
void StartCone(Target *t)
{
    struct pc_visual *v;
    f32 tmat[4][3];
    f32 *vel;
    VelRot *motion;
    GameCar *car;
    TargInfo *info;

    info = &targetInfo[t->infoIdx];
    car = PC_CAR(t->slot);
    if (gCarMines && info->objListIdx == OBJ_CONE) {
        car->carMine = 1;
        MBOX_HideObject(t->dlIdx, 0, 15);
        t->flags &= ~(TARGET_ACTIVE | TARGET_HIT);
        return;
    }
    if (gMineCones && info->objListIdx == OBJ_CONE) {
        MBOX_HideObject(t->dlIdx, 0, 15);
        StartBlast(t->pos, 0, 0.4f, 1);
        crash_car(PC_MODEL(t->slot));
        t->flags &= ~(TARGET_ACTIVE | TARGET_HIT);
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
    t->flags &= ~(TARGET_ACTIVE | TARGET_HIT);
    vel = N64P(t->data)->vel;
    motion = N64P(t->data);
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
    CopyUV(tmat, t->uvs);
    v->next = gEnvList;
    gEnvList = (struct tVisual *) v;
    PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->matPos, 2);
}
