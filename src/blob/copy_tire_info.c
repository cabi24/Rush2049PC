/* copy_tire_info.c: a tire's constants from the car's descriptor into the model (MAT3 initiali.c,
 * copy_tire_info()). Part of the car set-up (carsetup). */
#include "game/game.h"

/* Copy the six constants of descriptor tire tp1 (TireDesc: a physics descriptor's tires[] point at them) into model
 * tire tp2 (Tire, include/game/carphys.h), which begins with the same six. */
void copy_tire_info(TireDesc *tp1, Tire *tp2) {
    tp2->tradius = tp1->tradius;
    tp2->springK = tp1->springK;
    tp2->rubdamp = tp1->rubdamp;
    tp2->cstiff = tp1->cstiff;
    tp2->cfmax = tp1->cfmax;
    tp2->invmi = tp1->invmi;
}
