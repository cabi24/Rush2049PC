/* SetClutch.c: the automatic clutch (MAT3 mdrive.c, SetClutch()).
 *
 * The model thread sets each car's clutch from its engine speed and pedals before the physics step. */
#include "game/game.h"

/* The model record (tModelData), the fields SetClutch reads and writes. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x398];
    streetyp *lasttp[4];
    u8 pad3B0[0x5C];
    const s16 *torquecurve;
    u8 pad410[0x238];
    surftyp *movingSurface;
    u8 pad64C[0xD8];
    f32 clutch;                 /* +0x724 mainin.clutch: 1 disengaged (pedal down), 0 engaged */
    f32 brake;                  /* +0x728 mainin.brake 0..1 */
    f32 throttle;               /* +0x72C mainin.throttle 0..1 */
    u8 pad730[0x7D0 - 0x730];
    s16 rpm;                    /* +0x7D0 the engine's rpm (negative in reverse) */
} tModelData;

/* Set model m's clutch: pressed at idle (below 1000 rpm with less than clutch_idle_throttle of gas, MAT3 0.1) so the
 * engine doesn't stall; under hard braking (more than clutch_brake_threshold, MAT3 0.8) pressed below 4000 rpm,
 * released from 5000, and slipping in between (clutch_rpm_slope per rpm, MAT3 0.001); released otherwise. */
void SetClutch(tModelData *m) {
    s32 rpm = m->rpm;
    s32 revs;
    if (rpm >= 0) revs = rpm; else revs = -rpm;
    if (revs < 1000 && m->throttle < clutch_idle_throttle) m->clutch = 1.0f;
    else if (m->brake > clutch_brake_threshold) {
        if (revs < 4000) m->clutch = 1.0f;
        else if (revs >= 5000) m->clutch = 0.0f;
        else m->clutch = (5000 - revs) * clutch_rpm_slope;
    } else m->clutch = 0.0f;
}
