/* autoshift.c: the automatic gearbox (MAT3 drivetra.c, autoshift()).
 *
 * The drivetrain (drivetrain.c) calls it each step for a car with automatic transmission. */
#include "game/game.h"


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
