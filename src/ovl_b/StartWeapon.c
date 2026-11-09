#include "game/game.h"

typedef struct tVisual tVisual;
struct tVisual {
    tVisual *next;      /* 0x00 */
    s16 index;          /* 0x04 */
    s16 objnum;         /* 0x06 */
    s16 slot;           /* 0x08 */
    u32 data;           /* 0x0C */
    f32 timeStamp;      /* 0x10 */
    void (*func)(tVisual *, s16); /* 0x14 */
};

typedef struct tCarParts {
    s32 frame;                      /* 0x00 */
    u8 unknown04[0x14];
    s32 weap;                       /* 0x18 */
    u8 unknown1c[0x24];
} tCarParts;                        /* 0x40 */

VIEW(tCarParts[], gCarParts);
#define gCarParts gCarParts_view
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern f32 weaponOffsets[8][13][3];
VIEW(volatile f32, gSeconds);
#define gSeconds gSeconds_view

void StartWeapon(s16 slot)
{
    tVisual *v;

    v = &PC_CAR_VISUALS(slot)[20];
    if (v->objnum < 0) {
        v->index = 0;
        v->func = AnimateWeapon;
        v->timeStamp = gSeconds;
        v->slot = slot;
        v->objnum = gCarParts[slot].weap =
            InitCarObj(gObjList[217], 15, gCarParts[slot].frame, 0x2000, slot, -1, 1);
        ZOID_UpdateObject(v->objnum, weaponOffsets[1][PC_MODEL_BODY(slot)], 0);
    }
}
