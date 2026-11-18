/* engine.c: the engine's torque for one step of the car simulation (MAT3 drivetra.c, engine()).
 *
 * The drivetrain (drivetrain.c) calls it each model step: the torque comes off the car's torque curve at the
 * engine's speed and the throttle, then two modifiers apply. */
#include "game/game.h"

/* The engine's torque this step, into m->engtorque. enginetorque looks it up from the engine's speed in rpm
 * (engangvel times engine_rpm_scale: 60 / 2 pi, MAT3's 9.549305) and the throttle as 0..128. A crashed car's
 * engine gives its closed-throttle torque; reverse has its own rpm scale. Both rear tires (2 and 3) on dirt
 * or water give half as much torque again, and the automatic transmission costs autotrans_torque_scale (MAT3 0.925)
 * in every gear but fourth, the top one. */
void engine(tModelData *m)
{
  if (m->crashflag != 0) {
    m->engtorque = enginetorque(m, (s16) (s32) (m->engangvel * engine_rpm_scale_crashed), 0, m->torquecurve);
  } else if (m->gear < 0) {
    m->engtorque = enginetorque(m, (s16) (s32) (m->engangvel * engine_rpm_scale_reverse), (s16) (s32) (m->throttle * 128.0f), m->torquecurve);
  } else {
    m->engtorque = enginetorque(m, (s16) (s32) (m->engangvel * engine_rpm_scale), (s16) (s32) (m->throttle * 128.0f), m->torquecurve);
  }
  if ((m->roadcode[2] == ROADCODE_DIRT || m->roadcode[2] == ROADCODE_WATER) && (m->roadcode[3] == ROADCODE_DIRT || m->roadcode[3] == ROADCODE_WATER)) {
    m->engtorque = m->engtorque * 1.5f;
  }
  if (m->autotrans != 0 && m->gear != 4) {
    m->engtorque = m->engtorque * autotrans_torque_scale;
  }
}
