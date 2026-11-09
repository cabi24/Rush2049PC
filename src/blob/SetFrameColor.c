/* SetFrameColor.c: the colour of a car's frame, hood and tires (MAT3 visuals.c, SetFrameColor()). */
#include "game/game.h"

/* gCarParts[slot]: the car's scene objects (MAT3 CarParts, the N64's 0x40 bytes: CreateCar.c), each an s32 handle
 * read by its low half: the second s16 of the word on the N64, the first on a little-endian host
 * (docs/port/ENDIAN.md). */
typedef struct CarParts {
    short frame; short frame_high;
    char extraparts[12];
    short hood; short hood_high;
    char sheen_weap[8];
    struct { short low; short high; } tires[4];
    char sparks[20];
} CarParts;
VIEW(CarParts[], gCarParts);
#define gCarParts gCarParts_view

/* Give car `slot`'s frame, hood and four tires the colour *col (an RGBA word; CreateCar gives white), as MAT3's six
 * ZOID_SetColor calls do, written straight into the nodes. */
void SetFrameColor(short slot, unsigned int *col) {
    CarParts *car = &gCarParts[slot];
    short frame, hood, tire0, tire1, tire2, tire3;
    frame = car->frame; ObjInstanceList[frame].color = *col;
    hood = car->hood; ObjInstanceList[hood].color = *col;
    tire0 = car->tires[0].low; ObjInstanceList[tire0].color = *col;
    tire1 = car->tires[1].low; ObjInstanceList[tire1].color = *col;
    tire2 = car->tires[2].low; ObjInstanceList[tire2].color = *col;
    tire3 = car->tires[3].low; ObjInstanceList[tire3].color = *col;
}
