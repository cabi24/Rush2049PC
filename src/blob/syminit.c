/* syminit.c: a car's simulation set up from scratch (MAT3 drivsym.c, syminit()).
 *
 * Init_MDrive calls it when a car starts a race or respawns. */
#include "game/game.h"

/* Set model m up (initialize: the state, the car's constants, the road, the collision box), reset its controls
 * (MAT3 _InitModelControls: no steering, clutch or brake, throttle 1, first gear), find the road under it and place
 * its tires (road, positions), and give the sounds its rpm: engangvel in rpm (9.549305 = 60 / 2 pi) times 0.9
 * (MAT3 mcommunication). */
void syminit(tModelData *m)
{
    initialize(m);
    m->mainin.wheel = 0.0f;
    m->mainin.clutch = 0.0f;
    m->mainin.brake = 0.0f;
    m->mainin.throttle = 1.0f;
    m->mainin.gear = 1;
    road(m);
    positions(m);
    m->rpm = (s16) TRUNC_W (m->engangvel * 9.549305f * .9f);
}
