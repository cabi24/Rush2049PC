#include "game/game.h"

typedef struct {
    u8 pad0[8];
    f32 unk8;
} TorqueSrc;

/* PC: the record is the car's model (D_8014A250), which keeps its N64 layout: its tune pointer at +4 is a u32 window
 * address (docs/port/CARS.md 2.2). */
enum n64p_TorqueSrc { n64p_TorqueSrc_ };
typedef struct EngineState {
    u8 pad0[4];
    enum n64p_TorqueSrc unk4;
    u8 pad8[3];
    s8 unkB;
    s8 unkC;
} EngineState;
#define N64P(x) _Generic((x), enum n64p_TorqueSrc: (TorqueSrc *)PC_N64P(x), default: (x))

#define RPM_SCALE 1150

s16 enginetorque(EngineState *m, s16 rpm, s16 throttle, s16 *torquecurve)
{
    s16 rindex, tindex, rrem, trem, left, right;
    s16 *low_ptr, *hi_ptr;

    rindex = rpm / (s16)RPM_SCALE;
    rrem = rpm % (s16)RPM_SCALE;
    if (rindex < 0) {
        rindex = 0;
        rrem = 0;
    }
    if (rindex >= 11) {
        rindex = 10;
        rrem = RPM_SCALE - 1;
    }
    tindex = throttle / (s16)14;
    trem = throttle % (s16)14;
    if (tindex >= 9) {
        tindex = 8;
        trem = 13;
    }
    low_ptr = (s16 *)torquecurve + (tindex * 12) + rindex;
    hi_ptr = (s16 *)torquecurve + (tindex * 12) + rindex + 1;
    left = interp((*low_ptr), (*hi_ptr), rrem, RPM_SCALE - 1);
    low_ptr = (s16 *)torquecurve + ((tindex + 1) * 12) + rindex;
    hi_ptr = (s16 *)torquecurve + ((tindex + 1) * 12) + rindex + 1;
    right = interp((*low_ptr), (*hi_ptr), rrem, RPM_SCALE - 1);
    right = interp(left, right, trem, 14);
    right *= N64P(m->unk4)->unk8 * carEngineTorque[m->unkC][m->unkB];
    return right;
}
