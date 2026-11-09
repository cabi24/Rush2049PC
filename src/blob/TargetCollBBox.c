#include "game/game.h"

typedef union {
    struct { f32 x, y, z; } c;
    f32 v[3];
} Vector3;
typedef struct CollisionObject { char unknown00[0x65]; s8 index; } CollisionObject;
typedef struct {
    char unknown00[0xF4];
    f32 corners[4][3];
    char unknown124[0x654-0x124];
    f32 radius;
    char unknown658[0x794-0x658];
    f32 position[3];
    f32 basis[3][3];
    char unknown7C4[0x7EA-0x7C4];
    s8 collidable;
    char unknown7EB[0x808-0x7EB];
} CollisionModel;
typedef struct {
    f32 xmin, xmax, zmax, zmin, ymax, ymin, radius;
    f32 mass;
} CollisionBounds;
typedef struct CollisionReckon {
    unsigned int body;
    f32 basis[3][3];
    f32 position[3];
    f32 velocity[3];
} CollisionReckon;
#define BODY(r) ((CollisionBounds *)PC_N64P((r)->body))
VIEW(CollisionReckon *, gBBoxModelStructs);
#define gBBoxModelStructs gBBoxModelStructs_view

s32 TargetCollBBox(CollisionObject *object, s16 *index, void *arg2, s32 arg3) {
    CollisionModel *car;
    CollisionReckon *other;
    s32 mnum;
    f32 radius;
    f32 radius_squared;
    f32 delta[3];
    Vector3 world;
    f32 local[3], distance;
    s32 i, hit;
    mnum = *index;
    car = PC_MODEL(mnum);
    other = &gBBoxModelStructs[object->index];
    if (!car->collidable) {
        return 0;
    }
    delta[0] = car->position[0] - other->position[0];
    delta[1] = car->position[1] - other->position[1];
    delta[2] = car->position[2] - other->position[2];
    for (i = 0, distance = 0.0; i < 3; i++) {
        distance += delta[i] * delta[i];
    }
    radius = BODY(other)->radius + car->radius;
    radius_squared = radius * radius;
    if (distance > radius_squared) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        WorldVector(car->corners[i], world.v, car->basis[0]);
        world.c.x = car->position[0] + world.c.x;
        world.c.y = car->position[1] + world.c.y;
        world.c.z = car->position[2] + world.c.z;
        world.c.x -= other->position[0];
        world.c.y -= other->position[1];
        world.c.z -= other->position[2];
        BodyVector(world.v, local, other->basis[0]);
        if (local[0] > BODY(other)->xmax || local[0] < BODY(other)->xmin) {
            hit = 0;
        } else if (local[1] > BODY(other)->ymax || local[1] < BODY(other)->ymin) {
            hit = 0;
        } else if (local[2] > BODY(other)->zmax || local[2] < BODY(other)->zmin) {
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
