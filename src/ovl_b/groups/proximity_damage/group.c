/* ProximityDamage: an explosion damages the cars near it (MAT3 shots.c). */
#include "game/game.h"

extern f32 fabsf(f32);

/* Damages every car (not crashed, dead or not collidable) within sqrt(`radius_squared`) of `origin`: `magnitude`
 * times the square of how near the centre it is, for car `source_index` (BattleDamage); a car carrying weapon 5
 * takes a third of it from the front (the blast's direction from `origin` to `endpoint`). */
void ProximityDamage(f32 *origin, f32 *endpoint, s32 source_index, f32 radius_squared, s32 magnitude)
{
    tCarData *target;
    f32 direction[3];
    f32 transformed[3];
    tCarData *source;
    f32 delta[3];
    f32 distance_squared;
    for (target = pc_cars; target < &pc_cars[gNumCars]; target++) {
        if (!PC_MODEL_CRASHFLAG(target->slot) && target->collidable && !target->dead) {
            delta[0] = origin[0] - target->dr_pos[0];
            delta[1] = origin[1] - target->dr_pos[1];
            delta[2] = origin[2] - target->dr_pos[2];
            distance_squared = delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2];
            if (distance_squared < radius_squared) {
                distance_squared = (radius_squared - distance_squared) / radius_squared;
                source = &pc_cars[source_index];
                if (target->battle.weapon.type == 5) {
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
