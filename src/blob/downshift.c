/* downshift.c: the automatic gearbox's shift down (MAT3 drivetra.c, downshift()). */
#include "game/game.h"

/* The model record (tModelData), the fields downshift reads and writes. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x398];
    streetyp *lasttp[4];
    u8 pad3B0[0x44];
    s16 gear;                   /* +0x3F4 -1 reverse, 0 neutral, 1..4 */
    u8 pad3F6[0x40C - 0x3F6];
    const s16 *torquecurve;
    u8 pad410[0x648 - 0x410];
    surftyp *movingSurface;
    u8 pad64C[0x658 - 0x64C];
    f32 shifttime;              /* +0x658 the model time before which no other shift happens */
    u8 pad65C[0x714 - 0x65C];
    f32 modelTime;              /* +0x714 fastin.modelTime, seconds */
} tModelData;

/* Shift model m down a gear, unless it is in first (or neutral or reverse) or shifted less than a second ago. */
void downshift(tModelData *m) {
    f32 model_time;
    s16 gear;

    gear = m->gear;
    if (gear >= 2) {
        model_time = m->modelTime;
        if (m->shifttime < model_time) {
            m->gear = gear - 1;
            m->shifttime = model_time + 1.0f;
        }
    }
}
