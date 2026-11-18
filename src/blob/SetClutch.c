/* SetClutch.c: the automatic clutch (MAT3 mdrive.c, SetClutch()).
 *
 * The model thread sets each car's clutch from its engine speed and pedals before the physics step. */
#include "game/game.h"

/* Set model m's clutch: pressed at idle (below 1000 rpm with less than clutch_idle_throttle of gas, MAT3 0.1) so the
 * engine doesn't stall; under hard braking (more than clutch_brake_threshold, MAT3 0.8) pressed below 4000 rpm,
 * released from 5000, and slipping in between (clutch_rpm_slope per rpm, MAT3 0.001); released otherwise. */
void SetClutch(tModelData *m) {
    s32 rpm = m->rpm;
    s32 revs;
    if (rpm >= 0) revs = rpm; else revs = -rpm;
    if (revs < 1000 && m->mainin.throttle < clutch_idle_throttle) m->mainin.clutch = 1.0f;
    else if (m->mainin.brake > clutch_brake_threshold) {
        if (revs < 4000) m->mainin.clutch = 1.0f;
        else if (revs >= 5000) m->mainin.clutch = 0.0f;
        else m->mainin.clutch = (5000 - revs) * clutch_rpm_slope;
    } else m->mainin.clutch = 0.0f;
}
