/* CalcDrift.c: a car's tire drift factor (MAT3 initiali.c, CalcDrift()).
 *
 * One of the set-up helpers carsetup calls to fill the model record from the car's parts. */
#include "game/game.h"

/* The model record (tModelData), the car's parts it reads. */
typedef struct tModelData {
    u8 pad0[8];
    u8 body_type;       /* +0x008 the car, 0..12 */
    s8 trans;           /* +0x009 */
    s8 autotrans;       /* +0x00A */
    s8 handling;        /* +0x00B the handling (tune) choice */
    s8 engine_type;     /* +0x00C */
    s8 tire_type;       /* +0x00D */
} tModelData;

/* The drift (MAT3 tModelData.drift) of model m: gTireDrift by its tire type and handling, raised for a car with
 * soft suspension (gCarSuspensions 0) and lowered for a stiff one (2). */
f32 CalcDrift(tModelData *m) {
    f32 drift;
    s8 suspension;

    drift = gTireDrift[m->tire_type][m->handling];
    suspension = gCarSuspensions[m->body_type];
    if (suspension == 0) {
        drift += drift_soft_suspension_bonus;
    } else if (suspension == 2) {
        drift -= drift_stiff_suspension_penalty;
    }
    return drift;
}
