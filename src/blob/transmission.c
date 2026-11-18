/* transmission.c: the gearbox's ratios for the gear the car is in (MAT3 drivetra.c, transmission()).
 *
 * The drivetrain (drivetrain.c) calls it each step, after the gear may have changed. */
#include "game/game.h"


/* Model m's gearbox ratio, its transmission's ratio for its gear (carTransGearRatio[trans][gear + 1], column 0
 * reverse) times the tune's trans_scale, and the total ratio to the wheels, that times the descriptor's dwratio. */
void transmission(tModelData *m)
{
  f32 (*gear_ratio)[6] = (f32 (*)[6]) &carTransGearRatio;
  m->transratio = m->tune->trans_scale * gear_ratio[m->trans][m->gear + 1];
  m->totalratio = m->car->dwratio * m->transratio;
}
