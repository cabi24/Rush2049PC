/* StartSparks.c: give a car its five spark visuals (MAT3 visuals.c, StartSparks()).
 *
 * StartCar calls it for every car: visuals 1-5 become the sparks at the left front, left rear, right front and right
 * rear corners and under the car (their animators are in groups/sparks). */
#include "game/game.h"

/* The car record (tCarData), its visuals (struct pc_visual: index, objnum, slot, data, timeStamp, func). */
typedef struct tCarData {
    u8 pad0[0x110]; struct pc_visual visuals[21]; u8 pad308[0x380 - 0x308]; void *player; u8 pad384[0x3B8 - 0x384];
} tCarData;

/* Set car `slot`'s visuals 1-5 to the five sparks, each not sparking yet (index -1) and drawing its spark object.
 * The attract demo (gGameState 8: GS_DEMO) has no sparks. */
void StartSparks(s16 slot) {
    tCarData *car;
    CarParts *parts;

    if (gGameState & GS_DEMO) {
        return;
    }
    car = PC_CAR(slot);
    parts = &gCarParts[slot];
    car->visuals[1].func = AnimateLFSpark;
    car->visuals[1].index = -1;
    car->visuals[1].slot = slot;
    car->visuals[1].objnum = parts->lfSpark;
    car->visuals[2].func = AnimateLRSpark;
    car->visuals[2].index = -1;
    car->visuals[2].slot = slot;
    car->visuals[2].objnum = parts->lrSpark;
    car->visuals[3].func = AnimateRFSpark;
    car->visuals[3].index = -1;
    car->visuals[3].slot = slot;
    car->visuals[3].objnum = parts->rfSpark;
    car->visuals[4].func = AnimateRRSpark;
    car->visuals[4].index = -1;
    car->visuals[4].slot = slot;
    car->visuals[4].objnum = parts->rrSpark;
    car->visuals[5].func = AnimateBSpark;
    car->visuals[5].index = -1;
    car->visuals[5].slot = slot;
    car->visuals[5].objnum = parts->bSpark;
}
