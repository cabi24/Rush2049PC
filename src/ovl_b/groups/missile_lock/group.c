/* MissileLock: a missile homes on a car (MAT3 shots.c). */
#include "game/game.h"

typedef struct Object {
    struct Object *next;
    s8 owner, unknown05, kind, flags;
    f32 lifetime, timer, elapsed, velocity[3], position[3], previous[3], uv[3][3];
} Object;

extern f32 sqrtf(f32);

/* Turns missile `object` (yaw 0.04 and pitch 0.03 a 30 Hz frame) towards the nearest car of another team in the
 * 90-degree cone ahead of it within 2000 feet: not its owner's, nor crashed, dead or not collidable. */
void MissileLock(Object *object)
{
    tCarData *player;
    f32 local[3];
    f32 delta[3];
    f32 best[3];
    f32 nearest, distance;
    nearest = 2000.0f;
    for (player = pc_cars; player < &pc_cars[gNumCars]; player++) {
        if (object->owner != player->slot &&
            gCarTeam[object->owner] != gCarTeam[player->slot] &&
            !pc_models[player->slot].crashflag && player->collidable && !player->dead) {
            delta[0] = player->dr_pos[0] - object->position[0];
            delta[1] = player->dr_pos[1] - object->position[1];
            delta[2] = player->dr_pos[2] - object->position[2];
            BodyVector(delta, local, &object->uv[0][0]);
            if (!(local[2] < 0.0f || local[0] < -local[2] || local[0] > local[2] ||
                local[1] < -local[2] || local[1] > local[2])) {
                distance = sqrtf(local[0]*local[0] + local[1]*local[1] + local[2]*local[2]);
                if (!(distance >= nearest)) {
                    nearest = distance;
                    best[0] = local[0];
                    best[1] = local[1];
                    best[2] = local[2];
                }
            }
        }
    }
    if (nearest < 2000.0f) {
        if (best[0] > 0.0f) YawUV(PC_FSCALE(-0.04f), object->uv);
        else YawUV(PC_FSCALE(0.04f), object->uv);
        if (best[1] > 0.0f) PitchUV(PC_FSCALE(0.03f), object->uv);
        else PitchUV(PC_FSCALE(-0.03f), object->uv);
    }
}
