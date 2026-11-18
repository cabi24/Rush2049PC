/* transmission.c: the gearbox's ratios for the gear the car is in (MAT3 drivetra.c, transmission()).
 *
 * The drivetrain (drivetrain.c) calls it each step, after the gear may have changed. */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), its final drive. */
typedef struct tCar {
  u8 pad0[0x60];
  TireDesc *tires[4];           /* +0x60 each tire's constants */
  u8 pad70[0xA4 - 0x70];
  f32 dwratio;                  /* +0xA4 the differential's ratio */
} tCar;
/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its gearbox scale. */
typedef struct tTune {
  u8 pad0[0xC];
  f32 trans_scale;              /* +0x0C */
} tTune;

/* Model m's gearbox ratio, its transmission's ratio for its gear (carTransGearRatio[trans][gear + 1], column 0
 * reverse) times the tune's trans_scale, and the total ratio to the wheels, that times the descriptor's dwratio. */
void transmission(tModelData *m)
{
  f32 (*gear_ratio)[6] = (f32 (*)[6]) &carTransGearRatio;
  m->transratio = m->tune->trans_scale * gear_ratio[m->trans][m->gear + 1];
  m->totalratio = m->car->dwratio * m->transratio;
}
