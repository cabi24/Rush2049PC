/* SetFrameColor.c: the colour of a car's frame, hood and tires (MAT3 visuals.c, SetFrameColor()). */
#include "game/game.h"

/* Give car `slot`'s frame, hood and four tires the colour *col (an RGBA word; CreateCar gives white), as MAT3's six
 * ZOID_SetColor calls do, written straight into the nodes (a handle's low half is its node). */
void SetFrameColor(short slot, unsigned int *col) {
    CarParts *car = &gCarParts[slot];
    short frame, hood, tire0, tire1, tire2, tire3;
    frame = car->frame; ObjInstanceList[frame].color = *col;
    hood = car->hood; ObjInstanceList[hood].color = *col;
    tire0 = car->tires[0]; ObjInstanceList[tire0].color = *col;
    tire1 = car->tires[1]; ObjInstanceList[tire1].color = *col;
    tire2 = car->tires[2]; ObjInstanceList[tire2].color = *col;
    tire3 = car->tires[3]; ObjInstanceList[tire3].color = *col;
}
