/* ProximityDamage: an explosion damages the cars near it (MAT3 shots.c). */
#include "game/game.h"

/* The car record (MAT3 tCarData; the PC's pc_car, host layout): the fields this reads. */
typedef struct Player {
    u8 unknown000[8]; f32 position[3];
    u8 unknown014[0x110 - 0x14]; struct pc_visual visuals[21]; s8 collidable;
    u8 unknown309[80]; s8 dead;
    u8 unknown35A; s8 slot;
    u8 unknown35C[0x380 - 0x35C]; void *input; s8 weaponType;
    u8 unknown385[51];
} Player;

extern f32 fabsf(f32);

/* Damages every car (not crashed, dead or not collidable) within sqrt(`radius_squared`) of `origin`: `magnitude`
 * times the square of how near the centre it is, for car `source_index` (BattleDamage); a car carrying weapon 5
 * takes a third of it from the front (the blast's direction from `origin` to `endpoint`). */
void ProximityDamage(f32 *origin, f32 *endpoint, s32 source_index, f32 radius_squared, s32 magnitude)
{
    Player *target;
    f32 direction[3];
    f32 transformed[3];
    Player *source;
    f32 delta[3];
    f32 distance_squared;
    for (target = PC_CARS; target < PC_CARS_END(gNumCars); target++) {
        if (!PC_MODEL_CRASHFLAG(target->slot) && target->collidable && !target->dead) {
            delta[0] = origin[0] - target->position[0];
            delta[1] = origin[1] - target->position[1];
            delta[2] = origin[2] - target->position[2];
            distance_squared = delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2];
            if (distance_squared < radius_squared) {
                distance_squared = (radius_squared - distance_squared) / radius_squared;
                source = PC_CAR(source_index);
                if (target->weaponType == 5) {
                    direction[0] = endpoint[0] - origin[0];
                    direction[1] = endpoint[1] - origin[1];
                    direction[2] = endpoint[2] - origin[2];
                    BodyVector(direction, transformed, (f32 *)((u8 *)target + 0x2c));
                    if (fabsf(fatan2(transformed[0], transformed[2])) < 1.35f) {
                        BattleDamage(source, target, distance_squared*distance_squared*(f32)magnitude*0.35f);
                    } else {
                        BattleDamage(source, target, distance_squared*distance_squared*(f32)magnitude);
                    }
                } else {
                    BattleDamage(source, target, distance_squared*distance_squared*(f32)magnitude);
                }
            }
        }
    }
}
