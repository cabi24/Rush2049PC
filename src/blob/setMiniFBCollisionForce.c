/* setMiniFBCollisionForce.c: the force between a car and a track object it hit (MAT3 collision.c,
 * setMiniFBCollisionForce()).
 *
 * The track objects that cars collide with as bodies (TargetCollBBox: the "mini" models, MAT3 tMiniModelData) have
 * a box and a weight; when a car's corner is inside an object's box this pushes the car back, and crashes it if
 * the push is hard enough. The object itself is not pushed. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    char pad0[0x124];
    f32 centerForce[3];         /* 0x124 the force on the body's centre this step, car frame */
    char pad130[0x5C4 - 0x130];
    f32 collisionmass;          /* 0x5C4 its mass in collisions */
    char pad5C8[0x63C - 0x5C8];
    f32 crashthreshold;         /* 0x63C the force that crashes it */
    s8 crashflag;               /* 0x640 */
    char pad641[0x788 - 0x641];
    f32 reckon_rwv[3];          /* 0x788 reckon.rwv: the velocity the collisions use */
    f32 reckon_rwr[3];          /* 0x794 reckon.rwr: the position */
    f32 reckon_uvs[3][3];       /* 0x7A0 reckon.uvs: the orientation */
    char pad7C4[2];
    s16 slot;                   /* 0x7C6 */
    char pad7C8[4];
    s8 drone_type;              /* 0x7CC DRONE_TYPE_HUMAN: a human's car */
} tModelData;

/* An object's collision box (MAT3 tBBoxColl): its sides in its own frame, feet, and its weight. */
typedef struct {
    f32 sides[4];               /* 0x00 left (x min), right (x max), front (z max), back (z min) */
    char pad10[0xC];            /* 0x10 top, bottom, radius */
    f32 weight;                 /* 0x1C */
} tBBoxColl;

/* A track object as a body: tMiniModelData (game/types.h), whose bboxInfo is its tBBoxColl's window address. */
#define BBOX(m2) ((tBBoxColl *)PC_N64P((m2)->bboxInfo))

extern f32 fabsf(f32);

/* Collide car `m` with object `m2`; `pos` is the car's corner that is inside the object's box, in the object's
 * frame, and `dir` the world direction from the object to the car. `m_unused` is MAT3's first parameter, the
 * car again (TargetCollBBox passes it as an integer), which the function doesn't read.
 *
 * In the object's frame the car's centre is `beside` it when within its front and back, `behind` it (or in front
 * of it) when within its left and right. A crashed car with its centre inside the box is just pushed out
 * (MiniForceApart). Else a car beside the object gets no force along z, one behind it none along x; along the
 * other axes the force opposes the relative velocity (2,500 for each foot a second) and is at least 4,000, its
 * sign set by the side of the object the corner is on, and more when the centre is within the box's extent on the
 * other axis: the corner's depth past the side, squared, times 10,000 along z and 20,000 along x. The vertical
 * force opposes the vertical relative velocity, 100 for each foot a second. The force, scaled by the object's
 * weight over the pair's mean mass, is taken off the car's centerForce in
 * its frame; the car crashes if it is above 0.8 of its crash threshold, or always in suicide mode 1 (and for a
 * human's car in mode 2), unless it is a battle or the car is dead (CAR_DEAD). */
void setMiniFBCollisionForce(s32 m_unused, tModelData *m, tMiniModelData *m2, f32 *dir, f32 *pos) {
    f32 force[3], temp[3], unused[3], rvel[3], cent[3];
    f32 mass;
    f32 scale;
    s32 beside;
    s32 hum_car;
    s32 behind;

    temp[0] = m->reckon_rwr[0] - m2->pos[0];
    temp[1] = m->reckon_rwr[1] - m2->pos[1];
    temp[2] = m->reckon_rwr[2] - m2->pos[2];
    BodyVector(temp, cent, m2->uvs[0]);
    beside = ((cent[2] < BBOX(m2)->sides[2]) && (cent[2] > BBOX(m2)->sides[3]));
    behind = ((cent[0] < BBOX(m2)->sides[1]) && (cent[0] > BBOX(m2)->sides[0]));
    if (beside && behind && m->crashflag) {
        MiniForceApart(m, dir);
        return;
    }
    temp[0] = m->reckon_rwv[0] - m2->vel[0];
    temp[1] = m->reckon_rwv[1] - m2->vel[1];
    temp[2] = m->reckon_rwv[2] - m2->vel[2];
    BodyVector(temp, rvel, m2->uvs[0]);

    if (beside && !behind) {
        force[2] = 0.0;
    } else {
        force[2] = rvel[2] * 2500.0f;
        if (pos[2] > 0.0f) {
            if (force[2] > -4000.0f) {
                force[2] = -4000.0f;
            }
            if (behind) {
                temp[2] = pos[2] - BBOX(m2)->sides[2];
                temp[2] *= 10000.0f * fabsf(temp[2]);
                if (temp[2] < force[2]) {
                    force[2] = temp[2];
                }
            }
        } else if (pos[2] < 0.0f) {
            if (force[2] < 4000.0f) {
                force[2] = 4000.0f;
            }
            if (behind) {
                temp[2] = pos[2] - BBOX(m2)->sides[3];
                temp[2] *= 10000.0f * fabsf(temp[2]);
                if (temp[2] > force[2]) {
                    force[2] = temp[2];
                }
            }
        }
    }

    if (behind && !beside) {
        force[0] = 0.0;
    } else {
        force[0] = rvel[0] * 2500.0f;
        if (pos[0] > 0.0f) {
            if (force[0] > -4000.0f) {
                force[0] = -4000.0f;
            }
            if (beside) {
                temp[0] = pos[0] - BBOX(m2)->sides[1];
                temp[0] *= 20000.0f * fabsf(temp[0]);
                if (temp[0] < force[0]) {
                    force[0] = temp[0];
                }
            }
        } else if (pos[0] < 0.0f) {
            if (force[0] < 4000.0f) {
                force[0] = 4000.0f;
            }
            if (beside) {
                temp[0] = pos[0] - BBOX(m2)->sides[0];
                temp[0] *= 20000.0f * fabsf(temp[0]);
                if (temp[0] > force[0]) {
                    force[0] = temp[0];
                }
            }
        }
    }

    force[1] = rvel[1] * 100.0f;
    mass = (m->collisionmass + BBOX(m2)->weight) * 0.5f;
    WorldVector(force, temp, m2->uvs[0]);
    BodyVector(temp, cent, m->reckon_uvs[0]);
    scale = BBOX(m2)->weight / mass;
    cent[0] *= scale;
    cent[1] *= scale;
    cent[2] *= scale;
    m->centerForce[0] -= cent[0];
    m->centerForce[1] -= cent[1];
    m->centerForce[2] -= cent[2];
    hum_car = (m->drone_type == 2);   /* DRONE_TYPE_HUMAN */
    if (gSuicideMode == 1 || (gSuicideMode == 2 && hum_car) || vector_magnitude(cent) > m->crashthreshold * 0.8f) {
        if (gRace != kBattle && m->crashflag == 0 && PC_CAR_STATE(m->slot) != CAR_DEAD) {
            m->crashflag = 1;
        }
    }
}
