/* SetNewPathIndex: moves a drone to another point of its drone path (MAT3 maxpath.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData): the field this reads. */
struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad_8[0x398];
    streetyp *lasttp[4];
    u8 pad_3B0[0x5C];
    const s16 *torquecurve;
    u8 pad_410[0x238];
    surftyp *movingSurface;
    u8 pad_64C[0x17A];
    s16 slot; /* 0x7C6 the car slot */
};

/* Puts the drone of model `m` on point `idx` of its drone path, as if it had just reached it: 20 feet along the
 * stretch from there, on its line, with no jump pending. */
void SetNewPathIndex(struct tModelData *m, s16 idx) {
    struct pc_car_nav *ctl;
    ctl = &PC_CAR_NAV(m->slot);
    ctl->idx = idx;
    ctl->interval_time = (f32) gModelTime;
    ctl->newIdx = -1;
    ctl->xrel = 20.0f;
    ctl->yrel = 0.0f;
}
