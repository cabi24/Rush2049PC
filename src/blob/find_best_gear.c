/* find_best_gear.c: the gear an automatic gearbox starts in (MAT3 drivetra.c, find_best_gear()). */
#include "game/game.h"

/* The model record (tModelData), the fields find_best_gear reads and writes. */
typedef struct tModelData {
    _tCar *car;                 /* +0x000 the physics descriptor */
    tTune *tune;                /* +0x004 the tune record */
    u8 pad8;
    s8 trans;                   /* +0x009 the transmission (a row of carTransGearRatio) */
    u8 padA[0x3A0 - 0x00A];
    streetyp *lasttp[4];
    u8 pad3B0[0x3F4 - 0x3B0];
    s16 gear;                   /* +0x3F4 -1 reverse, 0 neutral, 1..4 */
    u8 pad3F6[0x40C - 0x3F6];
    const s16 *torquecurve;
    u8 pad410[0x41C - 0x410];
    f32 dwangvel;               /* +0x41C the driven wheels' speed, radians a second */
} tModelData;

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
