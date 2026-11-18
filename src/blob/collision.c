/* collision.c: the collisions between cars (MAT3 collision.c, collision()).
 *
 * Each model step, for each car, against every later car in the model array: two cars close enough to touch are
 * tested corner by corner, and the first corner of one inside the other's body gets the pair a collision force
 * (setFBCollisionForce). */
#include "game/game.h"

typedef struct Vec3 { float x, y, z; } Vec3;

/* Collide model `m` with each later car in the race (later only, so each pair is tested once a step). Nothing
 * collides with collisions off (outside practice), when `m` is not collidable, or in a ghost race. For a pair whose
 * bounding spheres overlap (the distance squared against the sum of the radii squared), each of m's corners is
 * moved into the other car's frame and tested against its body with 5 feet to spare (PointInBody), then each of
 * the other's corners against m's body; at the first one inside, setFBCollisionForce gets the pair (the one hit
 * second), the vector between them and the corner in the hit car's frame, and the step's collision test for `m`
 * ends there: one collision a car a step. The walk stops after the race's cars (PC_RACE_CARS records; on the N64
 * the end of its 6). */
void collision(tModelData *m)
{
 tModelData *other;
 Vec3 delta, world, local;
 Vec3 *corner;
 float *value;
 float length;
 int offset;
 if (gNoCarCollisions && gRace != kPractice) return;
 if (!m->collidable) return;
 if (gRace == kGhost) return;
 for (other = m + 1; other < &pc_models[PC_RACE_CARS]; other++) {
  if (!other->in_game || !other->collidable) continue;
  delta.x = m->reckon.rwr[0] - other->reckon.rwr[0];
  delta.y = m->reckon.rwr[1] - other->reckon.rwr[1];
  delta.z = m->reckon.rwr[2] - other->reckon.rwr[2];
  length = 0.0f;
  for (value = (float *)&delta; value < (float *)(&delta + 1); value++) length += *value * *value;
  if (length > (m->colrad + other->colrad) * (m->colrad + other->colrad)) continue;
  for (offset = 0, corner = m->bodyR; offset < (int)sizeof(m->bodyR); offset += (int)sizeof(Vec3), corner++) {
   WorldVector(corner, &world, m->reckon.uvs);
   world.x = m->reckon.rwr[0] + world.x; world.y = m->reckon.rwr[1] + world.y; world.z = m->reckon.rwr[2] + world.z;
   world.x -= other->reckon.rwr[0]; world.y -= other->reckon.rwr[1]; world.z -= other->reckon.rwr[2];
   BodyVector(&world, &local, other->reckon.uvs);
   if (PointInBody(other, &local, 5.0f)) {
    setFBCollisionForce(m, m, other, &delta, &local); return;
   }
  }
  for (offset = 0, corner = other->bodyR; offset != (int)sizeof(other->bodyR); offset += (int)sizeof(Vec3), corner++) {
   WorldVector(corner, &world, other->reckon.uvs);
   world.x = other->reckon.rwr[0] + world.x; world.y = other->reckon.rwr[1] + world.y; world.z = other->reckon.rwr[2] + world.z;
   world.x -= m->reckon.rwr[0]; world.y -= m->reckon.rwr[1]; world.z -= m->reckon.rwr[2];
   BodyVector(&world, &local, m->reckon.uvs);
   if (PointInBody(m, &local, 5.0f)) {
    setFBCollisionForce(m, other, m, &delta, &local); return;
   }
  }
 }
}
