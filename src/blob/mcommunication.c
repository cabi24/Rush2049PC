/* mcommunication.c: the engine speed the rest of the game sees (MAT3 drivsym.c, mcommunication()). */
#include "game/game.h"

/* After a model step: model `m`'s rpm, the engine speed in revolutions a minute (rad_per_sec_to_rpm, 60 / 2 pi)
 * times mcommunication_rpm_scale (MAT3 0.9). */
void mcommunication(tModelData *m) {
    m->rpm = m->engangvel * rad_per_sec_to_rpm * mcommunication_rpm_scale;
}
