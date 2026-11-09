/* StartMine: the start function of a battle mine (MAT3 targets.c _StartMine). It runs when a car drives over a
 * mine another car dropped (CheckTargetCollision through targetInfo[].start): the mine damages the car, and the
 * mine's owner gets the credit. The mine's own animation (AnimateMine) then sees it stopped colliding and blows it
 * up. */
#include "game/game.h"

/* A track object (MAT3 tTarget, 0x70 bytes): a level-object node, kept in its N64 layout on the PC (its link and
 * its per-kind data are 32-bit window addresses, docs/port/POINTERS.md 10). Only the fields this file reads. */
typedef struct Target {
    u8 pad00[4];
    u8 flags;               /* 0x04 Flags: TARGET_ACTIVE */
    u8 pad05[11];
    s16 infoIdx;            /* 0x10 its kind: the index of its targetInfo record */
    u8 pad12[0x38 - 0x12];
    f32 matPos[3];          /* 0x38 mat's position, where its sounds play */
    f32 pos[3];             /* 0x44 its position */
    u8 pad50[0x5C - 0x50];
    s8 slot;                /* 0x5C the car that hit it */
    u8 pad5D[0x65 - 0x5D];
    s8 owner;               /* 0x65 the car that dropped it */
} Target;


/* The weapon (pc_car.kind, MAT3 battle.weapon.type) whose car takes less damage from the front: MAT3's shot code
 * makes the same exception for it. */
#define WEAPON_FRONT_ARMOUR 5
#define MINE_DAMAGE_FRONT_ARMOUR 280
#define MINE_DAMAGE 800

/* A car has driven over the mine: unless the car is already wrecked, it takes 800 damage (280 if it has the
 * front-armour weapon and the mine is ahead of it), the mine is spent, and the mine's start sound plays. */
void StartMine(Target *t)
{
    struct pc_car *car;
    f32 delta[3];
    f32 local[3];

    if (PC_MODEL_CRASHFLAG(t->slot) == 0) {
        car = PC_CAR(t->slot);
        if (car->kind == WEAPON_FRONT_ARMOUR) {
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
        t->flags &= ~TARGET_ACTIVE; /* AnimateMine blows up a mine without it */
        PlayTargetSound(targetInfo[t->infoIdx].sound, t->slot, t->matPos, 2);
    }
}
