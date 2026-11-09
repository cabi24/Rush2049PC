/* CalcOffroadTires.c: how well a car's tires take the dirt (MAT3 initiali.c, CalcOffroadTires()).
 *
 * One of the set-up helpers carsetup calls to fill the model record. */
#include "game/game.h"

/* The model record (tModelData), the car's part it reads. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x5];
    s8 tire_type;       /* +0x00D the tire choice */
} tModelData;

/* The off-road factor (MAT3 tModelData.offroadtires) of model m's tire type: 0.75 for type 4, 0.5 for 3, -0.25 for
 * 1, -0.5 for 2, 0 for the others. */
f32 CalcOffroadTires(tModelData *m) {
    s8 tire_type;

    tire_type = m->tire_type;
    if (tire_type == 4) {
        return 0.75f;
    }
    if (tire_type == 3) {
        return 0.5f;
    }
    if (tire_type == 1) {
        return -0.25f;
    }
    if (tire_type == 2) {
        return -0.5f;
    }
    return 0.0f;
}
