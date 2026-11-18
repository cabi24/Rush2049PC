/* autoshift.c: the automatic gearbox (MAT3 drivetra.c, autoshift()).
 *
 * The drivetrain (drivetrain.c) calls it each step for a car with automatic transmission. */
#include "game/game.h"

/* The model record (tModelData), the fields autoshift reads and writes. */
typedef struct tModelData {
    _tCar *car; tTune *tune;    /* +0x000 the physics descriptor, +0x004 its tune */
    u8 pad8[0x3A0 - 0x008]; streetyp *lasttp[4]; u8 pad3B0[0x3D0 - 0x3B0]; f32 throttle;            /* +0x3D0 0..1 */
    u8 pad3D4[0x3F4 - 0x3D4]; s16 gear,commandgear; /* +0x3F4 the gear, +0x3F6 the one the player asks for */
    u8 pad3F8[0x408 - 0x3F8]; f32 engangvel;       /* +0x408 the engine's speed, radians a second */
    const s16 *torquecurve;
    u8 tail[0x648 - 0x410];
    surftyp *movingSurface;
    u8 tail_64C[0x808 - 0x64C];
} tModelData;

/* Choose model m's gear. Neutral and reverse are the player's (commandgear 0 or -1) and taken as they are; in a
 * forward gear the box shifts up past the car's upshift engine speed and down below its downshift speed, both
 * scaled by the tune's shiftk and raised with the throttle (0.75 of them with no gas, all of them at full), so the
 * engine revs higher when pushed. Leaving neutral or reverse it first picks the best gear for the speed. */
void autoshift(tModelData *m)
{
    f32 modupshiftangvel,moddownshiftangvel,fact;
    fact=(m->throttle+3.0f)*.25f;
    modupshiftangvel=m->tune->shiftk*m->car->upshiftangvel*fact;
    moddownshiftangvel=m->tune->shiftk*m->car->downshiftangvel*fact;
    if (m->commandgear==0 || m->commandgear==-1) {
        m->gear=m->commandgear;
    } else {
        if (m->gear==0 || m->gear==-1)
            find_best_gear(m,modupshiftangvel,moddownshiftangvel);
        if (m->engangvel>modupshiftangvel) upshift(m);
        if (m->engangvel<moddownshiftangvel) downshift(m);
    }
}
