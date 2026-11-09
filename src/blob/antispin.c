/* antispin.c: the steering's yaw help and the anti-spin moment (MAT3 drivsym.c, antispin()).
 *
 * A stage of the physics step (sym), after the forces: arcade handling on top of the tire model. */
#include "game/game.h"

/* The tune record (MAT3 anon_diffbl_weight_scale_2c, gTune[handling]). */
typedef struct tTune { u8 pad0[0x24]; f32 antispin; /* +0x24 */ } tTune;
enum n64p_tTune { n64p_tTune_ };
/* The model record (tModelData), the fields antispin reads and writes. */
typedef struct tModelData {
    u8 pad0[4];
    enum n64p_tTune tune;       /* +0x004 the tune record (a window address) */
    u8 pad8[0x40 - 0x8];
    f32 v[3];                   /* +0x040 the velocity in the car's frame (x sideways, z forward), feet a second */
    f32 w[3];                   /* +0x04C the angular velocity in the car's frame (w[1]: the yaw rate) */
    u8 pad58[0x13C - 0x58];
    f32 centerMoment[3];        /* +0x13C the moment about the centre of mass */
    u8 pad148[0x3D4 - 0x148];
    f32 brake;                  /* +0x3D4 0..1 */
    u8 pad3D8[0x3F0 - 0x3D8];
    f32 magvel;                 /* +0x3F0 the speed, feet a second */
    u8 pad3F4[0x5AC - 0x3F4];
    f32 drift;                  /* +0x5AC the drift factor (CalcDrift) */
    u8 pad5B0[0x60C - 0x5B0];
    s32 roadcode[4];            /* +0x60C each tire's road code */
    u8 pad61C[0x720 - 0x61C];
    f32 wheel;                  /* +0x720 mainin.wheel, the steering -1..1 */
    u8 pad724[0x7CA - 0x724];
    s16 we_control;             /* +0x7CA a drone this machine drives */
    u8 pad7CC[0x7D4 - 0x7CC];
    s32 appearance;             /* +0x7D4 APPEAR_* */
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tTune: (tTune *)PC_N64P(x), default: (x))

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
                m->centerMoment[1] -= m->wheel * 12000.0f * drift;
            } else {
                m->centerMoment[1] -= m->wheel * speed * 120.0f * drift;
            }
        }
        if (!(m->v[2] < 0)) {
            if (m->we_control != 0) {
                if (m->v[0] * m->w[1] > 0.0f) {
                    m->centerMoment[1] -= m->v[0] * 100.0f;
                }
            } else {
                if (m->v[0] * m->w[1] > 0.0f && m->brake < 0.5f) {
                    m->centerMoment[1] -= m->v[0] * 100.0f * N64P(m->tune)->antispin;
                }
            }
        }
    }
}
