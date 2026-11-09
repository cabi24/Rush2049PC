/* CreateCar.c: make a car's scene objects (MAT3 visuals.c, CreateCar()).
 *
 * The race start (StartEnvironment) and the car screen make each car's frame, hood, sheen, tires and sparks here
 * and colour them; StartCar then gives them their animators. */
#include "game/game.h"

/* A colour (MAT3 tRGBA): gTrackColors' entries, read as a word. */
typedef union tRGBA { unsigned char c[4]; unsigned int rgba; } tRGBA;
/* A texture definition (MAT3 tTexture, 36 bytes): only its size matters here. */
typedef struct tTexture { u8 pad[36]; } tTexture;


void ZOID_SetTexture(s16 objnum, void *texdef, int bank);

/* Make the scene objects of car slot `slot`, shown in the windows of mask `camlink`: the frame (the root), the hood
 * and the sheen from the slot's part definitions (OBJLIST_CAR_PARTS), the four tires, and in a race (`for_game`) the
 * five spark objects. The sheen takes the track's colours in a race. The rim texture (gTireRims: the human's choice
 * by slot, row 0 for drones) goes only to the first tire's node, as in SetupCar and AnimateTire (MAT3 sets all
 * four); without the Expansion Pak, on tracks 0-5, a plain texture (16) instead. In a race the car's IR colour starts
 * as the track's first colour and its paint job is made (SetupCarColor). The car screen (`for_game` 0) uses the
 * screen player's preset matrices and slot numbers past 13 (slot % 13 is the car). */
void CreateCar(s16 slot, s16 camlink, int for_game)
{
    CarParts *car;
    s16 offset;
    unsigned int idx;
    unsigned int flags;
    unsigned int flags2;
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
    car->tires[0] = InitCarObj(gObjList[OBJLIST_TIRE], camlink, car->frame, flags2 | 0x2000, slot, 3, for_game);
    car->tires[1] = InitCarObj(gObjList[OBJLIST_TIRE], camlink, car->frame, flags2 | 0x2000, slot, 4, for_game);
    car->tires[2] = InitCarObj(gObjList[OBJLIST_TIRE], camlink, car->frame, flags2 | 0x2000, slot, 5, for_game);
    car->tires[3] = InitCarObj(gObjList[OBJLIST_TIRE], camlink, car->frame, flags2 | 0x2000, slot, 6, for_game);
    if (expansion_pak_present || gTrackNum >= 6) {
        ZOID_SetTexture(car->tires[0], gWheelTextures[TIRE_RIMS(PC_LINK_OWNER(idx) == 6 ? idx + 1 : 0)[idx] + 3], -1);
    } else ZOID_SetTexture(car->tires[0], gWheelTextures[16], -1);
    if (for_game) {
        car->lfSpark = InitCarObj(gObjList[OBJLIST_SPARK], camlink, car->frame, 0xf00, slot, 7, for_game);
        car->lrSpark = InitCarObj(gObjList[OBJLIST_SPARK], camlink, car->frame, 0xf00, slot, 8, for_game);
        car->rfSpark = InitCarObj(gObjList[OBJLIST_SPARK], camlink, car->frame, 0xf00, slot, 9, for_game);
        car->rrSpark = InitCarObj(gObjList[OBJLIST_SPARK], camlink, car->frame, 0xf00, slot, 10, for_game);
        car->bSpark = InitCarObj(gObjList[OBJLIST_SPARK], camlink, car->frame, 0xf00, slot, 11, for_game);
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
