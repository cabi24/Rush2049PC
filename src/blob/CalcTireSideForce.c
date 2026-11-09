/* CalcTireSideForce.c: a car's tire side force (MAT3 initiali.c, CalcTireSideForce()).
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

/* The side force factor of model m's tire type, gTireSideForce. */
f32 CalcTireSideForce(tModelData *m) {
    return gTireSideForce[m->tire_type];
}
