/* find_best_gear.c: the gear an automatic gearbox starts in (MAT3 drivetra.c, find_best_gear()). */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), its final drive. */
typedef struct tCar {
    u8 pad0[0xA4];
    f32 dwratio;                /* +0xA4 the differential's ratio */
} tCar;
/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its gearbox scale. */
typedef struct tTune {
    u8 pad0[0xC];
    f32 trans_scale;            /* +0x0C */
} tTune;
/* The model record (tModelData), the fields find_best_gear reads and writes. Its descriptor and tune pointers are
 * u32 window addresses: the record keeps its N64 layout (docs/port/CARS.md 4). */
typedef struct tModelData {
    u32 car;                    /* +0x000 the physics descriptor (tCar) */
    u32 tune;                   /* +0x004 the tune record (tTune) */
    u8 pad8;
    s8 trans;                   /* +0x009 the transmission (a row of carTransGearRatio) */
    u8 padA[0x3F4 - 0xA];
    s16 gear;                   /* +0x3F4 -1 reverse, 0 neutral, 1..4 */
    u8 pad3F6[0x41C - 0x3F6];
    f32 dwangvel;               /* +0x41C the driven wheels' speed, radians a second */
} tModelData;

/* Put model m in the lowest forward gear, from first to fourth, in which the engine would turn slower than usang (the
 * upshift speed autoshift computed) at the driven wheels' present speed: the engine speed is the wheels' times the
 * tune's trans_scale, the descriptor's dwratio and the gear's ratio. dsang (the downshift speed) is not used. */
void find_best_gear(tModelData *m, f32 usang, f32 dsang) {
    m->gear = 1;
    if (m->gear < 4) {
        do {
            f32 ratio = (((tTune *) PC_N64P(m->tune))->trans_scale * ((tCar *) PC_N64P(m->car))->dwratio) * carTransGearRatio[m->trans][m->gear+1];
            f32 engine_speed = m->dwangvel * ratio;
            if (engine_speed < usang) {
                break;
            }
            m->gear = (s16) (m->gear + 1);
        } while (m->gear < 4);
    }
}
