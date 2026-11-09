/* engine.c: the engine's torque for one step of the car simulation (MAT3 drivetra.c, engine()).
 *
 * The drivetrain (drivetrain.c) calls it each model step: the torque comes off the car's torque curve at the
 * engine's speed and the throttle, then two modifiers apply. */
#include "game/game.h"

/* The torque curve's 32-bit N64 address (the model's torquecurve) as the pointer at it (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3) */
#define CURVE(a) ((s16 *)PC_N64P(a))

/* The model record (tModelData, docs/port/CARS.md 4): the fields engine() reads and writes. */
typedef struct tModelData {
  u8 pad0[0xA]; s8 autotrans;                   /* +0x00A automatic transmission */
  u8 padB[0x3D0 - 0xB]; f32 throttle;           /* +0x3D0 0..1 */
  u8 pad3D4[0x3F4 - 0x3D4]; s16 gear;           /* +0x3F4 -1 reverse, 0 neutral, 1..4 */
  u8 pad3F6[0x404 - 0x3F6]; f32 engtorque;      /* +0x404 the result */
  f32 engangvel;                                /* +0x408 the engine's speed, radians a second */
  u32 torquecurve;                              /* +0x40C the torque curve's window address (stdtorquecurve) */
  u8 pad410[0x60C - 0x410]; s32 roadcode[4];    /* +0x60C each tire's road code (its road polygon's flags & 15) */
  u8 pad61C[0x640 - 0x61C]; s8 crashflag;       /* +0x640 crashed */
} tModelData;

/* The engine's torque this step, into m->engtorque. enginetorque looks it up from the engine's speed in rpm
 * (engangvel times engine_rpm_scale: 60 / 2 pi, MAT3's 9.549305) and the throttle as 0..128. A crashed car's
 * engine gives its closed-throttle torque; reverse has its own rpm scale. Both rear tires (2 and 3) on dirt
 * or water give half as much torque again, and the automatic transmission costs autotrans_torque_scale (MAT3 0.925)
 * in every gear but fourth, the top one. */
void engine(tModelData *m)
{
  if (m->crashflag != 0) {
    m->engtorque = enginetorque(m, (s16) (s32) (m->engangvel * engine_rpm_scale_crashed), 0, CURVE(m->torquecurve));
  } else if (m->gear < 0) {
    m->engtorque = enginetorque(m, (s16) (s32) (m->engangvel * engine_rpm_scale_reverse), (s16) (s32) (m->throttle * 128.0f), CURVE(m->torquecurve));
  } else {
    m->engtorque = enginetorque(m, (s16) (s32) (m->engangvel * engine_rpm_scale), (s16) (s32) (m->throttle * 128.0f), CURVE(m->torquecurve));
  }
  if ((m->roadcode[2] == ROADCODE_DIRT || m->roadcode[2] == ROADCODE_WATER) && (m->roadcode[3] == ROADCODE_DIRT || m->roadcode[3] == ROADCODE_WATER)) {
    m->engtorque = m->engtorque * 1.5f;
  }
  if (m->autotrans != 0 && m->gear != 4) {
    m->engtorque = m->engtorque * autotrans_torque_scale;
  }
}
