/* calcaa.c: angular acceleration from a moment (MAT3 drivsym.c, calcaa()). */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar, gCarModels[body], 0xB4 bytes): its moments of inertia. */
typedef struct tCar {
    f32 i[3];                   /* +0x00 the moments of inertia about x, y, z */
    f32 invi[3];                /* +0x0C their inverses */
} tCar;

/* angaccel = moment / inertia, axis by axis (the car's frame, whose axes are the principal axes of inertia). */
void calcaa(tModelData *m, f32 *moment, f32 *angaccel)
{
  tCar *car = m->car;
  angaccel[0] = moment[0] * car->invi[0];
  angaccel[1] = moment[1] * car->invi[1];
  angaccel[2] = moment[2] * car->invi[2];
}
