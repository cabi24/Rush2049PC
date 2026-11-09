#include "game/game.h"

typedef struct {
    u8 pad0[20];
    s32 model;      /* 20 */
    u8 pad24[40];
} CarPart;          /* 64 */

VIEW(CarPart[], gCarParts);
#define gCarParts gCarParts_view

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

void *MBOX_FindTexture_WORK(char *name, u16 *index, s8 lo, s8 hi, s32 err);
void ZOID_SetTexture(s16 object, void *tex, s32 mode);

void SetSheen(s16 part, s16 car, s16 tex)
{
    CarPart *p;
    void *t;
    u16 index;
    s16 mask;

    if (tex < -1 || tex >= 11) {
        return;
    }
    switch (car) {
    case 0:
        mask = 1;
        break;
    case 1:
        mask = 2;
        break;
    case 2:
        mask = 4;
        break;
    case 3:
        mask = 8;
        break;
    }
    if (tex == -1) {
        MBOX_HideObject(gCarParts[car * 13 + part].model, 0, mask);
    } else {
        t = MBOX_FindTexture_WORK(sheenName[tex], &index, 0, gNumModels - 1, 1);
        p = &gCarParts[car * 13 + part];
        ZOID_SetTexture(p->model, t, -1);
        MBOX_ShowObject(p->model, 0, mask);
    }
}
