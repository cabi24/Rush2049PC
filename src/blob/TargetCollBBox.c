/* TargetCollBBox: collision test 4 of the track objects (gTargetCollFuncPtrs[4], MAT3 targets.c _TargetCollBBox):
 * a car against a target's box, for the solid moving targets (in MAT3's table: boulders, elevators, gondolas,
 * trolleys). On a hit it applies the push itself (setMiniFBCollisionForce) and still reports no hit, so the
 * target's start function never runs. */
#include "game/game.h"

typedef union {
    struct { f32 x, y, z; } c;
    f32 v[3];
} Vector3;


/* The model record's view (MAT3 tModelData): what the test reads. */
typedef struct {
    _tCar *car;
    Tune44 *tune;
    char unknown08[0xF4-0x08];
    f32 corners[4][3];      /* 0xF4 bodyR: the body's four corners, car space */
    char unknown124[0x3A0-0x124];
    streetyp *lasttp[4];
    char unknown3B0[0x40C-0x3B0];
    const s16 *torquecurve;
    char unknown410[0x648-0x410];
    surftyp *movingSurface;
    char unknown64C[0x654-0x64C];
    f32 radius;             /* 0x654 colrad: the car's collision radius */
    char unknown658[0x794-0x658];
    f32 position[3];        /* 0x794 reckon.rwr: the dead-reckoned position */
    f32 basis[3][3];        /* 0x7A0 reckon.uvs: its orientation */
    char unknown7C4[0x7EA-0x7C4];
    s8 collidable;          /* 0x7EA */
    char unknown7EB[0x808-0x7EB];
} CollisionModel;


/* Test car *index against the box of target t: its bounding box model (gBBoxModelStructs[t->owner]: where it is,
 * which way it faces, its box, a row of BBoxCollInfo). Nothing happens if the car isn't collidable or its circle
 * doesn't reach the box's; else each of the car's four corners is taken into the box's space, and at the first
 * that lies inside the box the push is applied. Always returns 0. */
s32 TargetCollBBox(tTarget *t, s16 *index, void *unused3, s32 unused4) {
    CollisionModel *car;
    tMiniModelData *other;
    s32 mnum;
    f32 radius;
    f32 radius_squared;
    f32 delta[3];
    Vector3 world;
    f32 local[3], distance;
    s32 i, hit;
    mnum = *index;
    car = PC_MODEL(mnum);
    other = &gBBoxModelStructs[t->owner];
    if (!car->collidable) {
        return 0;
    }
    delta[0] = car->position[0] - other->pos[0];
    delta[1] = car->position[1] - other->pos[1];
    delta[2] = car->position[2] - other->pos[2];
    for (i = 0, distance = 0.0; i < 3; i++) {
        distance += delta[i] * delta[i];
    }
    radius = MINI_MODEL_BOX(other)->rad + car->radius;
    radius_squared = radius * radius;
    if (distance > radius_squared) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        WorldVector(car->corners[i], world.v, car->basis[0]);
        world.c.x = car->position[0] + world.c.x;
        world.c.y = car->position[1] + world.c.y;
        world.c.z = car->position[2] + world.c.z;
        world.c.x -= other->pos[0];
        world.c.y -= other->pos[1];
        world.c.z -= other->pos[2];
        BodyVector(world.v, local, other->uvs[0]);
        if (local[0] > MINI_MODEL_BOX(other)->right || local[0] < MINI_MODEL_BOX(other)->left) {
            hit = 0;
        } else if (local[1] > MINI_MODEL_BOX(other)->top || local[1] < MINI_MODEL_BOX(other)->bot) {
            hit = 0;
        } else if (local[2] > MINI_MODEL_BOX(other)->front || local[2] < MINI_MODEL_BOX(other)->back) {
            hit = 0;
        } else {
            hit = 1;
        }
        if (hit) {
            setMiniFBCollisionForce((intptr_t)car, car, other, delta, local);
            return 0;
        }
    }
    return 0;
}
