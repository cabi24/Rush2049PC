/* steer_constants.c: a car's steering constants (MAT3 initiali.c, steer_constants()). Part of the car set-up
 * (carsetup). */
#include "game/game.h"


/* Model m's steering gain, the road wheels' angle at full lock: the steering wheel's 540 degrees (one and a half
 * turns) over the descriptor's steering ratio, in radians (steer_deg_to_rad, MAT3 0.017453292); and its trail
 * (MAT3 trail): swtpg times the steering ratio over the weight times tire 2's position over the wheelbase. */
void steer_constants(tModelData *m)
{
  _tCar *car;
  car = m->car;
  m->steergain = (540.0f / car->steerratio) * steer_deg_to_rad;
  m->trail = (car->swtpg * car->steerratio) / (m->weight * (car->tireR[2][2] / m->wheelbase));
}
