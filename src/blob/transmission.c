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
/* The model record (tModelData), the fields transmission reads and writes. */
typedef struct tModelData {
  tCar *car;                    /* +0x000 the physics descriptor */
  tTune *tune;                  /* +0x004 the tune record */
  u8 pad8;
  s8 trans;                     /* +0x009 the transmission (a row of carTransGearRatio) */
  u8 padA[0x3A0 - 0x00A];
  streetyp *lasttp[4];
  u8 pad3B0[0x3F4 - 0x3B0];
  s16 gear;                     /* +0x3F4 -1 reverse, 0 neutral, 1..4 */
  u8 pad3F6[0x40C - 0x3F6];
  const s16 *torquecurve;
  u8 pad410[0x428 - 0x410];
  f32 transratio;               /* +0x428 the gearbox's ratio */
  f32 totalratio;               /* +0x42C the gearbox's and the differential's */
} tModelData;

/* Model m's gearbox ratio, its transmission's ratio for its gear (carTransGearRatio[trans][gear + 1], column 0
 * reverse) times the tune's trans_scale, and the total ratio to the wheels, that times the descriptor's dwratio. */
void transmission(tModelData *m)
{
  f32 (*gear_ratio)[6] = (f32 (*)[6]) &carTransGearRatio;
  m->transratio = m->tune->trans_scale * gear_ratio[m->trans][m->gear + 1];
  m->totalratio = m->car->dwratio * m->transratio;
}
