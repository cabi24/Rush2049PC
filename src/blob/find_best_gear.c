/* find_best_gear.c: the gear an automatic gearbox starts in (MAT3 drivetra.c, find_best_gear()). */
#include "game/game.h"


/* Put model m in the lowest forward gear, from first to fourth, in which the engine would turn slower than usang (the
 * upshift speed autoshift computed) at the driven wheels' present speed: the engine speed is the wheels' times the
 * tune's trans_scale, the descriptor's dwratio and the gear's ratio. dsang (the downshift speed) is not used. */
void find_best_gear(tModelData *m, f32 usang, f32 dsang) {
    m->gear = 1;
    if (m->gear < 4) {
        do {
            f32 ratio = (m->tune->trans_scale * m->car->dwratio) * carTransGearRatio[m->trans][m->gear+1];
            f32 engine_speed = m->dwangvel * ratio;
            if (engine_speed < usang) {
                break;
            }
            m->gear = (s16) (m->gear + 1);
        } while (m->gear < 4);
    }
}
