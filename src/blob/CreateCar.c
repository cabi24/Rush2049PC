/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * CreateCar (0x800B15B4, 357 words) = CreateCar, MAT3 GameSource/src/visuals.c (GC 0x80054648).
 * , : closes 34-word draft (music_seq_load/best.c; earlier 323/357).
 * N64 form: no extra-part, brakes/trim or per-tyre rim code; the sheen texture is gTexList[9] (N64 gTexList =
 * gTexList, 10 entries); SetSheenColor gets SheenColors[gTrackNum] in game, Track1Colors[0] otherwise; one rim
 * texture on tires[0] when expansion_pak_present || gTrackNum >= 6 (else rim 13, gWheelTextures[16]); IRclr/prevIRclr from
 * gTrackColors[gTrackNum][0]; SetupCarColor takes five arguments. InitCarObj = InitCarObj.
 * The lever that closed it: the texture index is the ARRAY element gTexList[9] (gTexList[9]), not the scalar
 * stand-in D_8016137A the drafts used. ugen evaluates `&gTexDefPtrs[t >> 10].texDefs[t & 1023]` index-side first
 * when t is an array element (base side first for a scalar), which is retail's temp order (t8/t9 for the index,
 * t1/t2/t3 for the table load) and every later temp name (34 words -> MATCH).
 * levers kept: flag words as OR chains of single bits (`flags2 | 0x2000 | 0x10 | 0x8`, `flags | 0x40000 |
 * 0x2000 | 0x4`), ZOID_SetTexture on gCarParts[slot].sheen, idx declared before flags, and the SetSheenColor /
 * ZOID_SetColor / ZOID_SetEnvColor struct forms (SetSheenColor / ZOID_SetColor / ZOID_SetEnvColor), defined here so
 * the standalone compile inlines them as the unit does.
 * Unused locals u0..u3: the exact frame residual (retail frame 192 with flags at sp+176 and spill homes at sp+56/64;
 * without them the frame is 176 and 20 words differ; 4 slots). Not explained yet (an inlined helper's slots?).
 * INTEGRATION: in the whole-program unit this body is EQUAL only with struct-form replacements of the locked
 * singles ZOID_SetColor, ZOID_SetEnvColor, SetSheenColor (the same replacement the
 * tire group requires). With the current locked m2c singles the unit gives 256/357 words differ.
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;

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
typedef struct ObjInstance {
    unsigned int flags; char pad4[0x38]; unsigned int color; unsigned int envcolor;
} ObjInstance;
typedef struct tTexDef { u8 pad[36]; } tTexDef;
typedef struct tTexDefPtr { tTexDef *texDefs; int pad; } tTexDefPtr;
typedef struct tLink { u8 pad0; u8 car; u8 pad2[5]; u8 owner; } tLink;
typedef struct tCarData { u8 pad0[0x110 - 0x0]; struct pc_visual visuals[21]; u8 pad0_308[0x34C - 0x308]; tRGBA IRclr; tRGBA prevIRclr; u8 pad354[0x360 - 0x354]; int sheenAlpha; u8 pad364[0x380 - 0x364]; void *input; u8 pad364_384[0x3B8 - 0x384]; } tCarData;
typedef struct tModelData { u8 pad0[0x7CD]; u8 a; u8 b; u8 c; u8 pad7D0[0x808 - 0x7D0]; } tModelData;

extern s16 gNumPlayers;
extern u16 gObjList[];                /* gObjList */
extern CarParts gCarParts[];           /* gCarParts */
extern u16 gTexList[];                 /* gTexList (N64: 10 entries) */
extern tTexDefPtr gTexDefPtrs[];
extern ObjInstance ObjInstanceList[];        /* ObjInstanceList */
extern tRGBA SheenColors[][2];           /* SheenColors */
extern tRGBA Track1Colors[];              /* Track1Colors */
extern tRGBA *gTrackColors[];             /* gTrackColors */
extern s8 gTrackNum;
extern s8 expansion_pak_present;
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gTireRims[][13];             /* gTireRims */
/* The wheel textures (InitVisuals): the rims from 3, by rim type. */
typedef struct TexDef TexDef;
extern TexDef *gWheelTextures[24];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

int InitCarObj(int objnum, s16 camlink, int parent, u32 flags, s16 slot, s16 carObjNum, int forGame);
void ZOID_SetTexture(s16 objnum, void *texdef, int bank);
void SetupCarColor(s16 slot, u8 car, u8 a, u8 b, u8 c);
void SetFrameColor(s16 slot, tRGBA *col);

void SetSheenColor(s16 slot, tRGBA *col, tRGBA *env);

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
