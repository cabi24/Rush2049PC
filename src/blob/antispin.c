#include "game/game.h"

typedef struct Sub { u8 p0[36]; f32 f36; } Sub;
enum n64p_Sub { n64p_Sub_ };
typedef struct Car {
    u8 p0[4];
    enum n64p_Sub sub;
    u8 p8[56];
    f32 f64;
    u8 p68[4];
    f32 f72;
    u8 p76[4];
    f32 f80;
    u8 p84[236];
    f32 f320;
    u8 p324[656];
    f32 f980;
    u8 p984[24];
    f32 f1008;
    u8 p1012[440];
    f32 f1452;
    u8 p1456[92];
    s32 w1548;
    s32 w1552;
    u8 p1556[268];
    f32 f1824;
    u8 p1828[166];
    s16 h1994;
    u8 p1996[8];
    s32 flags2004;
} Car;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Sub: (Sub *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

void antispin(Car *m) {
    f32 scale;
    f32 speed;

    if (!(m->flags2004 & 0x10)) {
        if (m->w1548 != 8 || m->w1552 != 8) {
            scale = m->f1452;
            scale += m->f980 * 0.5f;
            if (scale > 1.0f) {
                scale = 1.0f;
            }
            speed = m->f1008;
            if (speed > 100.0f) {
                m->f320 -= m->f1824 * 12000.0f * scale;
            } else {
                m->f320 -= m->f1824 * speed * 120.0f * scale;
            }
        }
        if (!(m->f72 < 0)) {
            if (m->h1994 != 0) {
                if (m->f64 * m->f80 > 0.0f) {
                    m->f320 -= m->f64 * 100.0f;
                }
            } else {
                if (m->f64 * m->f80 > 0.0f && m->f980 < 0.5f) {
                    m->f320 -= m->f64 * 100.0f * N64P(m->sub)->f36;
                }
            }
        }
    }
}
