#include "game/game.h"

/* : hand-derived from the retail words */
typedef struct SndSlot {         /* 0x18 bytes; gGameCar[p].snd[] starts at 0x110 */
    void *next;                  /* a car visual (tVisual): next is a host pointer */
    s16 f4;                      /* 0x04 */
    s16 handle;                  /* 0x06 */
    s16 slot;                    /* 0x08 */
    s16 pad0A;
    s32 idx;                     /* 0x0C */
    f32 t;                       /* 0x10 */
    void *cb;                    /* 0x14 */
} SndSlot;

typedef struct HudRec {          /* model[player], 0x808 bytes */
    u8 pad000[8];
    u8 b8;
    u8 pad009[0x7CA - 9];
    s16 s7CA;
    u8 pad7CC[0x808 - 0x7CC];
} HudRec;

VIEW(s8, gCarWings);
#define gCarWings gCarWings_view

intptr_t ZOID_FindMatrix(s16);

void StartWingFlame(s16 slot, s16 idx) {
    s8 (*wings)[13] = (s8 (*)[13]) &gCarWings;   /* gCarWings, s8[][13]: the wing option by row (player + 1) and car type */
    HudRec *h;
    s32 k;
    s16 hd;
    if (gRace == 2 || gRace == 6 || (h = PC_MODEL(slot), h->s7CA != 0)) {
        ((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x2C0))))[idx].cb = 0;
        return;
    }
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x2C0))))[idx])->cb = (void *) AnimateWingFlame;
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x2C0))))[idx])->slot = slot;
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x2C0))))[idx])->idx = idx;
    k = wings[slot + 1][h->b8] * 4 + idx + 0xE2;
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x2C0))))[idx])->handle = InitCarObj(gObjList[k], 0xF, ((SndSlot *) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290)))[idx].handle, 0, slot, -1, 1);
    hd = (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x2C0))))[idx])->handle;
    *(f32 *) (ZOID_FindMatrix(hd) + 0x28) = wing_flame_height;
    MBOX_HideObject((&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x2C0))))[idx])->handle, 1, 0xF);
}

intptr_t ZOID_FindMatrix(s16 arg0);
