/* downshift.c: the automatic gearbox's shift down (MAT3 drivetra.c, downshift()). */
#include "game/game.h"

/* Shift model m down a gear, unless it is in first (or neutral or reverse) or shifted less than a second ago. */
void downshift(tModelData *m) {
    f32 model_time;
    s16 gear;

    gear = m->gear;
    if (gear >= 2) {
        model_time = m->fastin.modelTime;
        if (m->shifttime < model_time) {
            m->gear = gear - 1;
            m->shifttime = model_time + 1.0f;
        }
    }
}
