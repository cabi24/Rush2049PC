/* setFBCollisionForce.c: the force between two colliding cars (MAT3 collision.c, setFBCollisionForce()).
 *
 * collision() calls it when a corner of one car is inside the other's body. Besides the push between the cars it
 * handles a car mine (a car carrying one crashes the other) and, in a battle, the ram attack (weapon 5), and it
 * crashes a car pushed hard enough. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    /* 0x000 */ _tCar *car;
    /* 0x004 */ Tune44 *tune;
    /* 0x008 */ char pad8[0xEC];
    /* 0x0F4 */ f32 bodyR[4][3];        /* the body's corners, car frame, feet: 0 front +x, 3 back -x */
    /* 0x124 */ f32 centerForce[3];     /* the force on the body's centre this step, car frame */
    /* 0x130 */ char pad130[0xF0];
    /* 0x220 */ f32 rwv[3];             /* the velocity in the world, feet a second */
    /* 0x22C */ char pad22C[0x174];
    /* 0x3A0 */ streetyp *lasttp[4];
    /* 0x3B0 */ char pad3B0[0x40];
    /* 0x3F0 */ f32 magvel;             /* the speed */
    /* 0x3F4 */ char pad3F4[0x18];
    /* 0x40C */ const s16 *torquecurve;
    /* 0x410 */ char pad410[0x1B4];
    /* 0x5C4 */ f32 collisionmass;      /* its mass in collisions */
    /* 0x5C8 */ char pad5C8[0x74];
    /* 0x63C */ f32 crashthreshold;     /* the force that crashes it */
    /* 0x640 */ s8 crashflag;
    /* 0x641 */ char pad641[0x7];
    /* 0x648 */ surftyp *movingSurface;
    /* 0x64C */ char pad64C[0x78];
    /* 0x6C4 */ s16 resurrect_moving_state;   /* RES_IDLE when not respawning */
    /* 0x6C6 */ char pad6C6[0xC2];
    /* 0x788 */ f32 reckon_rwv[3];      /* reckon.rwv: the velocity the collisions use */
    /* 0x794 */ f32 reckon_rwr[3];      /* reckon.rwr: the position */
    /* 0x7A0 */ f32 reckon_uvs[3][3];   /* reckon.uvs: the orientation */
    /* 0x7C4 */ s16 obResPt;
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ char pad7C8[4];
    /* 0x7CC */ s8 drone_type;          /* DRONE_TYPE_HUMAN: a human's car */
} tModelData;

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields used here. */
typedef struct tCarData {
    /* 0x000 */ char pad0[0x110 - 0x0]; struct pc_visual visuals[21]; char pad0_308[0x358 - 0x308];
    /* 0x358 */ s8 crash_state;         /* CRASH_NONE, CRASH_WRECKED, CRASH_RESPAWN */
    /* 0x359 */ s8 dead;                /* CAR_ABORTED, CAR_ALIVE, CAR_DYING, CAR_DEAD */
    /* 0x35A */ s8 in_tunnel;
    /* 0x35B */ s8 slot;
    /* 0x35C */ char pad35C[3];
    /* 0x35F */ s8 carMine;             /* it carries a car mine: the next car it hits crashes */
    /* 0x360 */ char pad360[0x380 - 0x360]; void *input;
    /* 0x384 */ s8 weapon_type;         /* battle.weapon.type: 5 the ram */
    /* 0x385 */ s8 weapon_ammoCnt;      /* battle.weapon.ammoCnt */
    /* 0x386 */ char pad386[0x26];
    /* 0x3AC */ f32 reloadingTime;      /* battle.reloadingTime: seconds until the weapon is ready again */
    /* 0x3B0 */ char pad3B0[8];
} tCarData;

f32 fabsf(f32);

/* Collide cars m1 and m2: a corner of one is inside the other's body, `pos` that corner in m2's frame, `dir` the
 * world vector between them; `m` is the car whose collision test found it (m1 or m2, for ForceApart). collision()
 * passes a sixth argument, the body test's 5-foot margin (MAT3 `top`), which this doesn't read.
 *
 * Outside battles, a car carrying a car mine crashes the other, losing the mine, and halves the other's velocity.
 * In m2's frame m1's centre is `beside` m2 when within m2's front and back (bodyR[0] and [3] z), `behind` it (or
 * in front) when within its left and right (x). In a battle between cars of different teams, a car with the ram
 * (weapon 5) that is ready (reloadingTime 0 or less), against a car that is not respawning or wrecked, with the
 * other car more than 2.25 feet ahead of it, hits it: 160 plus 7 for each foot a second of its speed in damage
 * (BattleDamage), a third of a second's reload and one ram used; m1 is tested first and only one car hits.
 * Cars whose centres overlap on both axes are pushed apart (ForceApart). Else the force on m2, in m2's frame:
 * along z 0 when beside, else the relative velocity times 2,000, at least 4,000 in size with its sign set by the
 * side of m2 the corner is on; along x 0 when behind, else likewise, and when beside also at least the corner's
 * depth past m2's side (bodyR[2] or [1] x), squared, times 20,000; vertically the relative velocity times 100. With
 * the pair's mean collision mass, m1 gets the force scaled by m2's mass (taken off its centerForce, in its own
 * frame) and m2 the force scaled by m1's (added to its own). Either car crashes when its share is above half its
 * crash threshold, or always in suicide mode 1 (in mode 2 when either car is a human's), unless it is a battle or
 * the car is dead (CAR_DEAD). */
void setFBCollisionForce(tModelData *m, tModelData *m1, tModelData *m2, f32 *dir, f32 *pos) {
    f32 force[3];
    f32 temp[3];
    f32 unused[3];
    f32 rvel[3];
    f32 cent[3];
    f32 cent2[3];
    f32 mass;
    f32 scale;
    s32 beside;
    s32 behind;
    s32 hum_car;
    s32 k;
    s32 damage;
    tCarData *gc1;
    tCarData *gc2;

    gc1 = PC_CAR(m1->slot);
    gc2 = PC_CAR(m2->slot);
    if (m1->slot >= 0 && m2->slot >= 0 && gRace != kBattle) {
        if (gc1->carMine) {
            m2->crashflag = 1;
            gc1->carMine = 0;
            m2->rwv[0] *= 0.5f;
            m2->rwv[1] *= 0.5f;
            m2->rwv[2] *= 0.5f;
        }
        if (gc2->carMine) {
            m1->crashflag = 1;
            gc2->carMine = 0;
            m1->rwv[0] *= 0.5f;
            m1->rwv[1] *= 0.5f;
            m1->rwv[2] *= 0.5f;
        }
    }

    temp[0] = m1->reckon_rwr[0] - m2->reckon_rwr[0];
    temp[1] = m1->reckon_rwr[1] - m2->reckon_rwr[1];
    temp[2] = m1->reckon_rwr[2] - m2->reckon_rwr[2];
    BodyVector(temp, cent, m2->reckon_uvs[0]);
    beside = (cent[2] < m2->bodyR[0][2]) && (cent[2] > m2->bodyR[3][2]);
    behind = (cent[0] < m2->bodyR[0][0]) && (cent[0] > m2->bodyR[3][0]);

    if (gRace == kBattle && gCarTeam[gc1->slot] != gCarTeam[gc2->slot]) {
        if (gc1->weapon_type == 5 && m2->resurrect_moving_state == RES_IDLE && gc1->reloadingTime <= 0 &&
            gc2->crash_state == CRASH_NONE) {
            temp[0] = m2->reckon_rwr[0] - m1->reckon_rwr[0];
            temp[1] = m2->reckon_rwr[1] - m1->reckon_rwr[1];
            temp[2] = m2->reckon_rwr[2] - m1->reckon_rwr[2];
            BodyVector(temp, cent2, m1->reckon_uvs[0]);
            if (cent2[2] > 2.25f) {
                gc1->reloadingTime = 0.3333333f;
                gc1->weapon_ammoCnt--;
                damage = 160;
                damage += m1->magvel * 7.0f;
                BattleDamage(gc1, gc2, damage);
            }
        } else if (gc2->weapon_type == 5 && m1->resurrect_moving_state == RES_IDLE && gc2->reloadingTime <= 0 &&
                   gc1->crash_state == CRASH_NONE) {
            if (cent[2] > 2.25f) {
                gc2->reloadingTime = 0.3333333f;
                gc2->weapon_ammoCnt--;
                damage = 160;
                damage += m2->magvel * 7.0f;
                BattleDamage(gc2, gc1, damage);
            }
        }
    }

    if (beside && behind) {
        ForceApart(m, m1, m2, dir);
        return;
    }

    temp[0] = m1->reckon_rwv[0] - m2->reckon_rwv[0];
    temp[1] = m1->reckon_rwv[1] - m2->reckon_rwv[1];
    temp[2] = m1->reckon_rwv[2] - m2->reckon_rwv[2];
    BodyVector(temp, rvel, m2->reckon_uvs[0]);

    if (beside) {
        force[2] = 0;
    } else {
        force[2] = rvel[2] * 2000.0f;
        if (pos[2] > 0.0f) {
            if (force[2] > -4000.0f) {
                force[2] = -4000.0f;
            }
        } else if (pos[2] < 0.0f) {
            if (force[2] < 4000.0f) {
                force[2] = 4000.0f;
            }
        }
    }

    if (behind) {
        force[0] = 0;
    } else {
        force[0] = rvel[0] * 2000.0f;
        if (pos[0] > 0.0f) {
            if (force[0] > -4000.0f) {
                force[0] = -4000.0f;
            }
            if (beside) {
                temp[0] = pos[0] - m2->bodyR[2][0];
                k = 20000;
                temp[0] *= k * fabsf(temp[0]);
                if (temp[0] < force[0]) {
                    force[0] = temp[0];
                }
            }
        } else if (pos[0] < 0.0f) {
            if (force[0] < 4000.0f) {
                force[0] = 4000.0f;
            }
            if (beside) {
                temp[0] = pos[0] - m2->bodyR[1][0];
                k = 20000;
                temp[0] *= k * fabsf(temp[0]);
                if (temp[0] > force[0]) {
                    force[0] = temp[0];
                }
            }
        }
    }

    force[1] = rvel[1] * 100.0f;
    mass = (m1->collisionmass + m2->collisionmass) * 0.5f;

    WorldVector(force, temp, m2->reckon_uvs[0]);
    BodyVector(temp, cent, m1->reckon_uvs[0]);
    scale = m2->collisionmass / mass;
    cent[0] *= scale;
    cent[1] *= scale;
    cent[2] *= scale;
    m1->centerForce[0] -= cent[0];
    m1->centerForce[1] -= cent[1];
    m1->centerForce[2] -= cent[2];
    hum_car = (m1->drone_type == 2) || (m2->drone_type == 2);   /* DRONE_TYPE_HUMAN */
    if (gSuicideMode == 1 || (gSuicideMode == 2 && hum_car) || vector_magnitude(cent) > m1->crashthreshold * 0.5f) {
        if (gRace != kBattle && m1->crashflag == 0 && gc1->dead != CAR_DEAD) {
            m1->crashflag = 1;
        }
    }

    scale = m1->collisionmass / mass;
    force[0] *= scale;
    force[1] *= scale;
    force[2] *= scale;
    m2->centerForce[0] = m2->centerForce[0] + force[0];
    m2->centerForce[1] = m2->centerForce[1] + force[1];
    m2->centerForce[2] = m2->centerForce[2] + force[2];
    if (gSuicideMode == 1 || (gSuicideMode == 2 && hum_car) || vector_magnitude(force) > m2->crashthreshold * 0.5f) {
        if (gRace != kBattle && m2->crashflag == 0 && gc2->dead != CAR_DEAD) {
            m2->crashflag = 1;
        }
    }
}
