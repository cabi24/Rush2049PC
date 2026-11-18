/* upshift.c: the automatic gearbox's shift up (MAT3 drivetra.c, upshift()). */
#include "game/game.h"

/* Shift model m up a gear, unless it is in fourth (the top) or shifted less than a second ago. */
void upshift(tModelData *m) {
    f32 model_time;
    s16 gear;

    gear = m->gear;
    if (gear < 4) {
        model_time = m->fastin.modelTime;
        if (m->shifttime < model_time) {
            m->gear = gear + 1;
            m->shifttime = model_time + 1.0f;
        }
    }
}
