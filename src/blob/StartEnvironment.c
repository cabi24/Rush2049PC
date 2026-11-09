/* StartEnvironment.c: make the race's cars and their shadows and arrows (MAT3 visuals.c, StartEnvironment(),
 * StartShadow(), StartArrow()). */
#include "game/game.h"

typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
/* A car visual (tVisual) on the PC: 0x20 bytes, next and func host pointers (docs/port/CARS.md 3). */
typedef struct tVisual {
    struct tVisual *next;       /* 0x00 */
    s16 index;                  /* 0x08 */
    s16 objnum;                 /* 0x0A */
    s16 slot;                   /* 0x0C */
    u8 pad0E[2];
    u32 data;                   /* 0x10 */
    float timeStamp;            /* 0x14 */
    void (*func)(struct tVisual *, s16);   /* 0x18 */
} tVisual;

typedef struct tPoly { s16 nverts; u16 flags; u16 texidx; s16 z; u8 verts[80]; } tPoly;

VIEW(tRGBA, gShadowColor);
#define gShadowColor gShadowColor_view
extern f32 gDummyVtx[4][3];            /* gDummyVtx */

VIEW(tPoly[], PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view

VIEW(tRGBA[5], arrowColors);
#define arrowColors arrowColors_view

tPoly *ZOID_NewPoly(int nverts, f32 (*xyz)[3], u16 texidx, tRGBA *color, u16 flags, int useTidx);

/* Give car `slot` its shadow: a four-cornered polygon (the shadow texture, gTexList[6], in gShadowColor at alpha
 * 192), which AnimateShadow lays on the road each frame. Its polygon number is the visual's objnum. MAT3 builds the
 * shadow from strips instead. */
static void StartShadow(s16 slot)
{
    tVisual *v;
    v = &PC_CAR_VISUALS(slot)[VIS_SHADOW];
    v->func = AnimateShadow;
    v->slot = slot;
    v->timeStamp = gModelTime;
    gShadowColor.c[3] = 192;
    v->objnum = ZOID_NewPoly(4, gDummyVtx, gTexList[6], &gShadowColor, ((gGameState & GS_CAR) ? 1 : 15) | 0x200 | 0x8000, 1) - PolyInstanceList;
}

/* Give a human's car `slot` in split screen its arrows: one polygon for each other player's window (window mask bit
 * i), in the car's colour (arrowColors; its team's in a battle), so the others can find it (AnimateArrow). The arrow
 * visuals left over below VIS_ARROWS_END are cleared. */
static void StartArrow(s16 slot)
{
    tVisual *v = &PC_CAR_VISUALS(slot)[VIS_ARROWS];
    int i;
    tRGBA clr = { { 255, 255, 255, 255 } };
    tPoly *poly;
    if (gRace == kBattle) {
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
        if (gRace == kBattle) poly = ZOID_NewPoly(4, gDummyVtx, gTexList[7], &clr, (1 << i) | 0x3200, 1);
        else poly = ZOID_NewPoly(4, gDummyVtx, gTexList[7], &clr, (1 << i) | 0x1200, 1);
        ZOID_SetPolyZ(poly, 1);
        v->objnum = poly - PolyInstanceList;
        v++;
    }
    while (v < (tVisual *) &PC_CAR_VISUALS(slot)[VIS_ARROWS_END]) {
        v->func = 0;
        v->objnum = -1;
        v++;
    }
}

/* At the race start, make every car in the race (PC_RACE_CARS: 6 on the N64) and start its visuals (CreateCar,
 * StartCar), then give each its shadow and, in split screen, each human's car its arrows (owner 6:
 * LINK_OWNER_HUMAN). */
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
