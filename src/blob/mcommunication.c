/* mcommunication.c: the engine speed the rest of the game sees (MAT3 drivsym.c, mcommunication()). */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    char pad[0x408];
    float engangvel;            /* 0x408 the engine's speed, radians a second */
    char pad40C[0x7D0 - 0x40C];
    s16 rpm;                    /* 0x7D0 the engine's speed for the sounds and the HUD */
} tModelData;

/* After a model step: model `m`'s rpm, the engine speed in revolutions a minute (rad_per_sec_to_rpm, 60 / 2 pi)
 * times mcommunication_rpm_scale (MAT3 0.9). */
void mcommunication(tModelData *m) {
    m->rpm = m->engangvel * rad_per_sec_to_rpm * mcommunication_rpm_scale;
}
