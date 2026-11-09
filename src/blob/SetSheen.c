/* SetSheen.c: the car screen's sheen over a car (MAT3 visuals.c, SetSheen()). */
#include "game/game.h"

/* gCarParts[slot]: the car's scene objects (MAT3 CarParts, the N64's 0x40 bytes: CreateCar.c). */
typedef struct {
    u8 pad0[20];
    s32 sheen;      /* 0x14 the environment-mapped shine over the body */
    u8 pad24[40];
} CarParts;         /* 64 */

VIEW(CarParts[], gCarParts);
#define gCarParts gCarParts_view

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

/* On the car screen, give car `slot` of screen player `player` (0-3; its cars are slots player * 13 + slot) the sheen
 * texture "SHEEN<sheen_index>" (sheenName, 0-10) and show it in that player's window, or hide the sheen there for
 * sheen_index -1. Any other index does nothing. */
void SetSheen(s16 slot, s16 player, s16 sheen_index)
{
    CarParts *car;
    void *tex;
    u16 texidx;
    s16 cam;

    if (sheen_index < -1 || sheen_index >= 11) {
        return;
    }
    switch (player) {
    case 0:
        cam = 1;
        break;
    case 1:
        cam = 2;
        break;
    case 2:
        cam = 4;
        break;
    case 3:
        cam = 8;
        break;
    }
    if (sheen_index == -1) {
        MBOX_HideObject(gCarParts[player * 13 + slot].sheen, 0, cam);
    } else {
        tex = MBOX_FindTexture_WORK(sheenName[sheen_index], &texidx, 0, gNumModels - 1, 1);
        car = &gCarParts[player * 13 + slot];
        ZOID_SetTexture(car->sheen, tex, -1);
        MBOX_ShowObject(car->sheen, 0, cam);
    }
}
