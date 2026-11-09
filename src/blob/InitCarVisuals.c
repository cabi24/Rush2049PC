/* InitCarVisuals.c: clear a car's visuals (MAT3 visuals.c, InitCarVisuals()). */
#include "game/game.h"

/* Clear the 21 visuals of the car record `car` (init_cars passes &pc_cars[i]): no animator, no scene object. On the
 * PC they are 0x28 bytes each from +0x110 (struct pc_visual, docs/port/CARS.md 2). */
void InitCarVisuals(struct tCarData *car) {
    s16 i;
    struct pc_visual *v;

    v = (struct pc_visual *) ((s8 *) car + 0x110);
    for (i = 0; i < CAR_VISUALS; i++) {
        v[i].func = 0;
        v[i].objnum = -1;
    }
}
