/* autoshift.c: the automatic gearbox (MAT3 drivetra.c, autoshift()).
 *
 * The drivetrain (drivetrain.c) calls it each step for a car with automatic transmission. */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), its shift points. */
typedef struct tCar {u8 pad0[0xAC]; f32 upshiftangvel,downshiftangvel; /* +0xAC, +0xB0 radians a second */} tCar;
/* The tune record (MAT3 anon_diffbl_weight_scale_2c). */
typedef struct tTune {u8 pad0[0x14]; f32 shiftk; /* +0x14 the shift points' scale */} tTune;
enum n64p_tCar { n64p_tCar_ };
enum n64p_tTune { n64p_tTune_ };
/* The model record (tModelData), the fields autoshift reads and writes. */
typedef struct tModelData {
    enum n64p_tCar car; enum n64p_tTune tune;     /* +0x000, +0x004 window addresses */
    u8 pad8[0x3D0 - 0x8]; f32 throttle;            /* +0x3D0 0..1 */
    u8 pad3D4[0x3F4 - 0x3D4]; s16 gear,commandgear; /* +0x3F4 the gear, +0x3F6 the one the player asks for */
    u8 pad3F8[0x408 - 0x3F8]; f32 engangvel;       /* +0x408 the engine's speed, radians a second */
    u8 tail[0x808 - 0x40C];
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tCar: (tCar *)PC_N64P(x), enum n64p_tTune: (tTune *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

/* Choose model m's gear. Neutral and reverse are the player's (commandgear 0 or -1) and taken as they are; in a
 * forward gear the box shifts up past the car's upshift engine speed and down below its downshift speed, both
 * scaled by the tune's shiftk and raised with the throttle (0.75 of them with no gas, all of them at full), so the
 * engine revs higher when pushed. Leaving neutral or reverse it first picks the best gear for the speed. */
void autoshift(tModelData *m)
{
    f32 modupshiftangvel,moddownshiftangvel,fact;
    fact=(m->throttle+3.0f)*.25f;
    modupshiftangvel=N64P(m->tune)->shiftk*N64P(m->car)->upshiftangvel*fact;
    moddownshiftangvel=N64P(m->tune)->shiftk*N64P(m->car)->downshiftangvel*fact;
    if (m->commandgear==0 || m->commandgear==-1) {
        m->gear=m->commandgear;
    } else {
        if (m->gear==0 || m->gear==-1)
            find_best_gear(m,modupshiftangvel,moddownshiftangvel);
        if (m->engangvel>modupshiftangvel) upshift(m);
        if (m->engangvel<moddownshiftangvel) downshift(m);
    }
}
