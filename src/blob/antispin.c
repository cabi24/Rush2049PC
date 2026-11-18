/* antispin.c: the steering's yaw help and the anti-spin moment (MAT3 drivsym.c, antispin()).
 *
 * A stage of the physics step (sym), after the forces: arcade handling on top of the tire model. */
#include "game/game.h"


/* Add model m's arcade yaw moments, unless it is a hulk:
 * - with a front tire on the road, a yaw moment straight from the steering wheel (12000 times the wheel at 100 ft/s
 *   and up, 120 times wheel times speed below), times the drift factor plus half the brake (at most 1): the car
 *   turns into a slide when braking;
 * - going forward and yawing the way it is sliding (w[1] and v[0] of one sign: a spin starting), a moment of 100
 *   times the sideways speed against it: in full for a drone driven here, else times the tune's antispin and only
 *   below half brake. */
void antispin(tModelData *m) {
    f32 drift;
    f32 speed;

    if (!(m->appearance & APPEAR_HULK)) {
        if (m->roadcode[0] != ROADCODE_AIRBORNE || m->roadcode[1] != ROADCODE_AIRBORNE) {
            drift = m->drift;
            drift += m->brake * 0.5f;
            if (drift > 1.0f) {
                drift = 1.0f;
            }
            speed = m->magvel;
            if (speed > 100.0f) {
                m->centerMoment[1] -= m->mainin.wheel * 12000.0f * drift;
            } else {
                m->centerMoment[1] -= m->mainin.wheel * speed * 120.0f * drift;
            }
        }
        if (!(m->v[2] < 0)) {
            if (m->we_control != 0) {
                if (m->v[0] * m->w[1] > 0.0f) {
                    m->centerMoment[1] -= m->v[0] * 100.0f;
                }
            } else {
                if (m->v[0] * m->w[1] > 0.0f && m->brake < 0.5f) {
                    m->centerMoment[1] -= m->v[0] * 100.0f * m->tune->antispin;
                }
            }
        }
    }
}
