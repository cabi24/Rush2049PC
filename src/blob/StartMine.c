/* StartMine: the start function of a battle mine (MAT3 targets.c _StartMine). It runs when a car drives over a
 * mine another car dropped (CheckTargetCollision through targetInfo[].start): the mine damages the car, and the
 * mine's owner gets the credit. The mine's own animation (AnimateMine) then sees it stopped colliding and blows it
 * up. */
#include "game/game.h"



/* The weapon (pc_car.weapon_type, MAT3 battle.weapon.type) whose car takes less damage from the front: MAT3's shot code
 * makes the same exception for it. */
#define WEAPON_FRONT_ARMOUR 5
#define MINE_DAMAGE_FRONT_ARMOUR 280
#define MINE_DAMAGE 800

/* A car (the target's slot) has driven over the mine (the target t; its owner is the car that dropped it): unless
 * the car is already wrecked, it takes 800 damage (280 if it has the front-armour weapon and the mine is ahead of
 * it), the mine is spent, and the mine's start sound plays. */
void StartMine(tTarget *t)
{
    struct pc_car *car;
    f32 delta[3];
    f32 local[3];

    if (PC_MODEL_CRASHFLAG(t->slot) == 0) {
        car = PC_CAR(t->slot);
        if (car->weapon_type == WEAPON_FRONT_ARMOUR) {
            delta[0] = t->pos[0] - car->pos[0];
            delta[1] = t->pos[1] - car->pos[1];
            delta[2] = t->pos[2] - car->pos[2];
            BodyVector(delta, local, car->uvs[0]);
            if (local[2] >= 0.0f) {
                BattleDamage(PC_CAR(t->owner), car, MINE_DAMAGE_FRONT_ARMOUR);
            } else {
                BattleDamage(PC_CAR(t->owner), car, MINE_DAMAGE);
            }
        } else {
            BattleDamage(PC_CAR(t->owner), car, MINE_DAMAGE);
        }
        t->Flags &= ~TARGET_ACTIVE; /* AnimateMine blows up a mine without it */
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->mat.pos, 2);
    }
}
