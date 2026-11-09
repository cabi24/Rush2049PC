#include "game/game.h"

typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
typedef struct CarParts {
    int frame;                  /* 0x00 */
    int extraparts[3];          /* 0x04 */
    int hood;                   /* 0x10 */
    int sheen;                  /* 0x14 */
    int weap;                   /* 0x18 */
    int tires[4];               /* 0x1C */
    int lfSpark;                /* 0x2C */
    int lrSpark;                /* 0x30 */
    int rfSpark;                /* 0x34 */
    int rrSpark;                /* 0x38 */
    int bSpark;                 /* 0x3C */
} CarParts;

typedef struct tTexDef { u8 pad[36]; } tTexDef;
typedef struct tTexDefPtr { tTexDef *texDefs; int pad; } tTexDefPtr;

VIEW(CarParts[], gCarParts);
#define gCarParts gCarParts_view

VIEW(tTexDefPtr[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view

VIEW(tRGBA[][2], SheenColors);
#define SheenColors SheenColors_view
VIEW(tRGBA[], Track1Colors);
#define Track1Colors Track1Colors_view

VIEW(s8[][13], gTireRims);
#define gTireRims gTireRims_view
/* The wheel textures (InitVisuals): the rims from 3, by rim type. */
typedef struct TexDef TexDef;
VIEW(TexDef *[24], gWheelTextures);
#define gWheelTextures gWheelTextures_view
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

void ZOID_SetTexture(s16 objnum, void *texdef, int bank);

void CreateCar(s16 slot, s16 camlink, int for_game)
{
    CarParts *car;
    s16 offset;
    unsigned int idx;
    unsigned int flags;
    unsigned int flags2;
    int u0, u1, u2, u3;
    idx = slot % 13;
    flags = (gNumPlayers == 1 || !for_game) ? 0 : 5;
    offset = slot * 3;
    car = &gCarParts[slot];
    flags2 = !for_game ? 0x40004 : 0;
    car->frame = InitCarObj(gObjList[0 + offset], camlink, -1, flags2 | 0x2000 | 0x10 | 0x8, slot, 0, for_game);
    car->hood = InitCarObj(gObjList[1 + offset], camlink, car->frame, flags2 | 0x2000 | 0x80, slot, -1, for_game);
    car->sheen = InitCarObj(gObjList[2 + offset], camlink, car->frame, flags2 | 0x2000 | 0x4000 | 0x200000 | 0x80000 | 0x80, slot, -1, for_game);
    ZOID_SetTexture(gCarParts[slot].sheen, &gTexDefPtrs[gTexList[9] >> 10].texDefs[gTexList[9] & 1023], -1);
    if (!for_game) SetSheenColor(slot, &Track1Colors[0], &Track1Colors[0]);
    else SetSheenColor(slot, &SheenColors[gTrackNum][0], &SheenColors[gTrackNum][1]);
    if (!for_game) flags2 = flags | 0x40000 | 0x2000 | 0x4;
    else flags2 = flags | (gNumPlayers == 1 ? 0 : 5);
    car->tires[0] = InitCarObj(gObjList[156], camlink, car->frame, flags2 | 0x2000, slot, 3, for_game);
    car->tires[1] = InitCarObj(gObjList[156], camlink, car->frame, flags2 | 0x2000, slot, 4, for_game);
    car->tires[2] = InitCarObj(gObjList[156], camlink, car->frame, flags2 | 0x2000, slot, 5, for_game);
    car->tires[3] = InitCarObj(gObjList[156], camlink, car->frame, flags2 | 0x2000, slot, 6, for_game);
    if (expansion_pak_present || gTrackNum >= 6) {
        ZOID_SetTexture(car->tires[0], gWheelTextures[gTireRims[PC_LINK_OWNER(idx) == 6 ? idx + 1 : 0][idx] + 3], -1);
    } else ZOID_SetTexture(car->tires[0], gWheelTextures[16], -1);
    if (for_game) {
        car->lfSpark = InitCarObj(gObjList[194], camlink, car->frame, 0xf00, slot, 7, for_game);
        car->lrSpark = InitCarObj(gObjList[194], camlink, car->frame, 0xf00, slot, 8, for_game);
        car->rfSpark = InitCarObj(gObjList[194], camlink, car->frame, 0xf00, slot, 9, for_game);
        car->rrSpark = InitCarObj(gObjList[194], camlink, car->frame, 0xf00, slot, 10, for_game);
        car->bSpark = InitCarObj(gObjList[194], camlink, car->frame, 0xf00, slot, 11, for_game);
    } else {
        car->lfSpark = -1;
        car->lrSpark = -1;
        car->rfSpark = -1;
        car->rrSpark = -1;
        car->bSpark = -1;
    }
    if (for_game) {
        PC_CAR_SHEEN_ALPHA(slot) = 255;
        PC_CAR_IR_COLOR(slot) = gTrackColors[gTrackNum][0].rgba;
        PC_CAR_PREV_IR_COLOR(slot) = gTrackColors[gTrackNum][0].rgba;
        SetupCarColor(slot, PC_LINK_CAR(slot), PC_MODEL_COLOR0(slot), PC_MODEL_COLOR1(slot), PC_MODEL_COLOR2(slot));
    }
    SetFrameColor(slot, &Track1Colors[0]);
}
