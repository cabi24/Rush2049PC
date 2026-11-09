/* mcommunication.c: the engine speed the rest of the game sees (MAT3 drivsym.c, mcommunication()). */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    _tCar *car;                 /* 0x000 the physics descriptor */
    Tune44 *tune;               /* 0x004 its tune */
    char pad8[0x3A0 - 0x8];
    streetyp *lasttp[4];        /* 0x3A0 each tire's last node of the road's surface tree */
    char pad3B0[0x408 - 0x3B0];
    float engangvel;            /* 0x408 the engine's speed, radians a second */
    const s16 *torquecurve;     /* 0x40C the torque curve */
    char pad410[0x648 - 0x410];
    surftyp *movingSurface;     /* 0x648 the moving surface under the car */
    char pad64C[0x7D0 - 0x64C];
    s16 rpm;                    /* 0x7D0 the engine's speed for the sounds and the HUD */
} tModelData;

/* After a model step: model `m`'s rpm, the engine speed in revolutions a minute (rad_per_sec_to_rpm, 60 / 2 pi)
 * times mcommunication_rpm_scale (MAT3 0.9). */
void mcommunication(tModelData *m) {
    m->rpm = m->engangvel * rad_per_sec_to_rpm * mcommunication_rpm_scale;
}
