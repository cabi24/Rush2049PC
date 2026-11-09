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

typedef struct SndCtl {          /* gCarParts[player], 0x40 bytes */
    s32 first;
    u8 pad04[0x18];
    s32 vals[4];                 /* 0x1C */
    u8 pad2C[0x14];
} SndCtl;

typedef struct HudRec {          /* model[player], 0x808 bytes */
    u8 pad000[8];
    u8 b8;
    u8 pad009[0x7CA - 9];
    s16 s7CA;
    u8 pad7CC[0x808 - 0x7CC];
} HudRec;

VIEW(s8, gCarWings);
#define gCarWings gCarWings_view

VIEW(s32, wingOffsets);
#define wingOffsets wingOffsets_view

VIEW(s32, gCarParts);
#define gCarParts gCarParts_view

void StartWing(s16 slot, s16 idx) {
    f32 (*wing_offsets)[3] = (f32 (*)[3]) &wingOffsets;   /* wingOffsets, f32[][3]: a row per body type */
    s8 (*wings)[13] = (s8 (*)[13]) &gCarWings;   /* gCarWings, s8[][13]: the wing option by row (player + 1) and car type */
    HudRec *h;
    f32 vec[3];
    s32 k;
    s32 b8;

    if (gRace == 2 || gRace == 6 || (h = PC_MODEL(slot), h->s7CA != 0)) {
        ((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290))))[idx].cb = 0;
        return;
    }
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290))))[idx])->cb = (void *) AnimateWing;
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290))))[idx])->slot = slot;
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290))))[idx])->idx = idx;
    b8 = h->b8;
    vec[0] = 0.0f;
    vec[2] = 0.0f;
    vec[1] = wing_offsets[b8][1];
    k = wings[slot + 1][b8] * 4 + idx + 0xE0;
    (&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290))))[idx])->handle = InitCarObj(gObjList[k], 0xF, ((SndCtl *) &gCarParts)[slot].first, 0, slot, -1, 1);
    ZOID_UpdateObject((&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290))))[idx])->handle, vec, NULL);
    MBOX_HideObject((&((SndSlot *) ((uintptr_t) ((u8 *) PC_CAR(slot) + CAR_OFS(0x290))))[idx])->handle, 1, 0xF);
    InitWingSmoke(slot, idx);
}

/* StartCar is defined in startcar.c */

/* stand-in caller: keeps StartWingFlame out of line under -O3 */

/* stand-in caller: keeps StartWing out of line under -O3 */
