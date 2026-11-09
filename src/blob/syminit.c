#include "game/game.h"

typedef struct MODELDAT {
    char pad0[1032];
    f32 engangvel;              /* 0x408 */
    char pad1036[1824 - 1036];
    volatile f32 quat[4];       /* 0x720 */
    volatile s8 quatvalid;      /* 0x730 */
    char pad1841[2000 - 1841];
    s16 rpm;                    /* 0x7D0 */
} MODELDAT;

/* PC: the engine speed to s16, which a car's engine spinning past +-32767 rpm overruns (seen in a 12-car race,
 * docs/port/MORECARS.md 4); undefined in C. Through s32, whose low half the conversion keeps, as the N64's trunc.w.s
 * and sh do (docs/port/SANITIZE.md, particle_knot's V16). */
#define TRUNC_W (int)
void syminit(MODELDAT *m)
{
    initialize(m);
    m->quat[0] = 0.0f;
    m->quat[1] = 0.0f;
    m->quat[2] = 0.0f;
    m->quat[3] = 1.0f;
    m->quatvalid = 1;
    road(m);
    positions(m);
    m->rpm = (s16) TRUNC_W (m->engangvel * 9.549305f * .9f);
}
