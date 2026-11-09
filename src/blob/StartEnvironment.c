#include "game/game.h"

typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
typedef struct tVisual {
    struct tVisual *next;       /* 0x00 */
    s16 index;                  /* 0x04 */
    s16 objnum;                 /* 0x06 */
    s16 slot;                   /* 0x08 */
    u8 pad0A[2];
    u32 data;                   /* 0x0C: flags (a u32 on the PC) */
    float timeStamp;            /* 0x10 */
    void (*func)(struct tVisual *, s16);   /* 0x14 */
} tVisual;

#define VISUAL(n) ((n) - 1)

typedef struct tPoly { s16 nverts; u16 flags; u16 texidx; s16 z; u8 verts[80]; } tPoly;

VIEW(tRGBA, gShadowColor);
#define gShadowColor gShadowColor_view
extern f32 gDummyVtx[4][3];            /* gDummyVtx */

VIEW(tPoly[], PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view

VIEW(tRGBA[5], arrowColors);
#define arrowColors arrowColors_view

/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

tPoly *ZOID_NewPoly(int nverts, f32 (*xyz)[3], u16 texidx, tRGBA *color, u16 flags, int useTidx);

static void StartShadow(s16 slot)
{
    tVisual *v;
    int i;
    void *vv;
    void *shadow1;
    void *shadow2;
    void *shadow3;
    int u0, u1;
    v = &PC_CAR_VISUALS(slot)[VISUAL(11)];
    v->func = AnimateShadow;
    v->slot = slot;
    v->timeStamp = gModelTime;
    gShadowColor.c[3] = 192;
    v->objnum = ZOID_NewPoly(4, gDummyVtx, gTexList[6], &gShadowColor, ((gGameState & 0x100) ? 1 : 15) | 0x200 | 0x8000, 1) - PolyInstanceList;
}

static void StartArrow(s16 slot)
{
    tVisual *v = &PC_CAR_VISUALS(slot)[VISUAL(12)];
    int i;
    int u2;
    tRGBA clr = { { 255, 255, 255, 255 } };
    tPoly *poly;
    if (gRace == 6) {
        clr.c[0] = arrowColors[gCarTeam[slot]].c[0];
        clr.c[1] = arrowColors[gCarTeam[slot]].c[1];
        clr.c[2] = arrowColors[gCarTeam[slot]].c[2];
        clr.c[3] = arrowColors[gCarTeam[slot]].c[3];
    } else {
        clr.c[0] = arrowColors[slot].c[0];
        clr.c[1] = arrowColors[slot].c[1];
        clr.c[2] = arrowColors[slot].c[2];
        clr.c[3] = arrowColors[slot].c[3];
    }
    for (i = 0; i < gNumWindows; i++) {
        if (i == slot) continue;
        v->func = AnimateArrow;
        v->slot = slot;
        v->index = i;
        if (gRace == 6) poly = ZOID_NewPoly(4, gDummyVtx, gTexList[7], &clr, (1 << i) | 0x3200, 1);
        else poly = ZOID_NewPoly(4, gDummyVtx, gTexList[7], &clr, (1 << i) | 0x1200, 1);
        ZOID_SetPolyZ(poly, 1);
        v->objnum = poly - PolyInstanceList;
        v++;
    }
    while (v < &PC_CAR_VISUALS(slot)[VISUAL(14)]) {
        v->func = 0;
        v->objnum = -1;
        v++;
    }
}

void StartEnvironment(void)
{
    s16 i;
    for (i = 0; i < PC_RACE_CARS; i++) {
        if (PC_MODEL_ACTIVE(i)) {
            CreateCar(i, 15, 1);
            StartCar(i);
        }
    }
    for (i = 0; i < PC_RACE_CARS; i++) {
        if (PC_MODEL_ACTIVE(i)) {
            StartShadow(i);
            if (gNumPlayers > 1 && PC_LINK_OWNER(i) == 6) StartArrow(i);
        }
    }
}
