/* collision.c: the collisions between cars (MAT3 collision.c, collision()).
 *
 * Each model step, for each car, against every later car in the model array: two cars close enough to touch are
 * tested corner by corner, and the first corner of one inside the other's body gets the pair a collision force
 * (setFBCollisionForce). */
#include "game/game.h"

typedef struct Vec3 { float x, y, z; } Vec3;

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    u8 pad0[0xF4];
    Vec3 bodyR[4];              /* 0x0F4 the body's four corners, car frame, feet */
    u8 pad124[0x654 - 0x124];
    float colrad;               /* 0x654 the radius of the sphere around the car, feet */
    u8 pad658[0x794 - 0x658];
    Vec3 reckon_rwr;            /* 0x794 reckon.rwr: the position the collisions use */
    float reckon_uvs[9];        /* 0x7A0 reckon.uvs: and the orientation */
    u8 pad7C4[0x7C8 - 0x7C4];
    s16 in_game;                /* 0x7C8 in the race */
    u8 pad7CA[0x7EA - 0x7CA];
    s8 collidable;              /* 0x7EA the others collide with it */
    u8 pad7EB[0x808 - 0x7EB];
} tModelData;

/* Collide model `m` with each later car in the race (later only, so each pair is tested once a step). Nothing
 * collides with collisions off (outside practice), when `m` is not collidable, or in a ghost race. For a pair whose
 * bounding spheres overlap (the distance squared against the sum of the radii squared), each of m's corners is
 * moved into the other car's frame and tested against its body with 5 feet to spare (PointInBody), then each of
 * the other's corners against m's body; at the first one inside, setFBCollisionForce gets the pair (the one hit
 * second), the vector between them and the corner in the hit car's frame, and the step's collision test for `m`
 * ends there: one collision a car a step. */
void collision(tModelData *m)
{
 tModelData *other;
 Vec3 delta, world, local;
 Vec3 *corner;
 float *value;
 float length;
 int offset;
 if (gNoCarCollisions && gRace != 1) return;   /* kPractice */
 if (!m->collidable) return;
 if (gRace == 2) return;   /* kGhost */
 for (other = m + 1; other < PC_MODELS_END; other++) {
  if (!other->in_game || !other->collidable) continue;
  delta.x = m->reckon_rwr.x - other->reckon_rwr.x;
  delta.y = m->reckon_rwr.y - other->reckon_rwr.y;
  delta.z = m->reckon_rwr.z - other->reckon_rwr.z;
  length = 0.0f;
  for (value = (float *)&delta; value < (float *)(&delta + 1); value++) length += *value * *value;
  if (length > (m->colrad + other->colrad) * (m->colrad + other->colrad)) continue;
  for (offset = 0, corner = m->bodyR; offset < (int)sizeof(m->bodyR); offset += (int)sizeof(Vec3), corner++) {
   WorldVector(corner, &world, m->reckon_uvs);
   world.x = m->reckon_rwr.x + world.x; world.y = m->reckon_rwr.y + world.y; world.z = m->reckon_rwr.z + world.z;
   world.x -= other->reckon_rwr.x; world.y -= other->reckon_rwr.y; world.z -= other->reckon_rwr.z;
   BodyVector(&world, &local, other->reckon_uvs);
   if (PointInBody(other, &local, 5.0f)) {
    setFBCollisionForce(m, m, other, &delta, &local); return;
   }
  }
  for (offset = 0, corner = other->bodyR; offset != (int)sizeof(other->bodyR); offset += (int)sizeof(Vec3), corner++) {
   WorldVector(corner, &world, other->reckon_uvs);
   world.x = other->reckon_rwr.x + world.x; world.y = other->reckon_rwr.y + world.y; world.z = other->reckon_rwr.z + world.z;
   world.x -= m->reckon_rwr.x; world.y -= m->reckon_rwr.y; world.z -= m->reckon_rwr.z;
   BodyVector(&world, &local, m->reckon_uvs);
   if (PointInBody(m, &local, 5.0f)) {
    setFBCollisionForce(m, other, m, &delta, &local); return;
   }
  }
 }
}
