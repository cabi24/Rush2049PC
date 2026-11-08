/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitVisuals (historical label): N64 InitDynamicObjs (arcade game/visuals.c), extended.
 *   - converts a 32-entry RGB byte palette (paint_colors_rgb) to RGBA5551 words with alpha set;
 *   - sets four bytes at gCarTeamColor (11, 5, 26, 16);
 *   - for_game == 0: return (select screens);
 *   - clears 3328 bytes at gCarParts; with gGameState & 0x100 calls the overlay routine at
 *     0x80391470 and clears gObjList (gObjList, s16) entries 0..155, otherwise runs the
 *     car-part lookups per link slot (InitCarDynamicObjs = the arcade car-part loop, 13 slots,
 *     model from gCarModelNums) and clears gCarPaintJobInited[slot];
 *   - looks up 24 rim textures (MBOX_FindTexture_Sub = MBOX_FindTexture_WORK, err 0) and stores the
 *     TEXDEF pointer (table gTexDefPtrs[index >> 10] + (index & 0x3FF)) or 0;
 *   - arcade `for (; i < MAX_TRK_OBJS; ++i) gObjList[i] = MBOX_FindObject(TrkNames[i -
 *     NUM_DYN_OBJS])`, here one entry (156) via MBOX_FindObject_WORK (MBOX_FindObject_Sub, WARN);
 *   - looks up the 10 spark/shadow/skid textures into gTexList.
 * Shaping quirks (compile-affecting): the arcade declaration list `i, j, k, model` plus an
 * unused buffer (>= 13 bytes; `partName[16]` here) for the 112-byte frame with `tex` at sp+94;
 * the car-slot loop uses `i` and the lookup loops `k` (with one shared variable the slot
 * counter loses s0 to the lookup pointers); the clear loop uses `j`; `k != 157` (with `<`
 * the one-entry loop is controlled by the name pointer); InitCarDynamicObjs is declared with
 * an s16 model so the caller loads it with lh.  Also matches at -O2.
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32;

#define GPACK_RGBA5551(r, g, b, a) ((((r) << 8) & 0xf800) | (((g) << 3) & 0x7c0) | (((b) >> 2) & 0x3e) | ((a) & 0x1))

typedef struct TexDef { u8 pad[36]; } TexDef;
typedef struct TexTable { TexDef *defs; u32 pad; } TexTable;
typedef struct CarInfo { u8 pad0; u8 car; u8 pad2[6]; } CarInfo;
typedef struct Rgb { u8 r, g, b, a; } Rgb;
typedef struct Four { u8 a, b, c, d; } Four;

extern Rgb paint_colors_rgb[32];
extern u32 paint_colors[32];
extern Four gCarTeamColor;
extern u8 gCarParts[];
extern u32 gGameState;
extern s16 gObjList[];
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s16 gCarModelNums[];
extern u8 gCarPaintJobInited[];
extern char *RimNames[24];
extern TexDef *gWheelTextures[24];
extern TexTable gTexDefPtrs[];
extern volatile u8 gNumModels;
extern char *tire_object_names[];
extern char *TexNames[10];
extern s16 gTexList[10];

extern void *memset(void *, s32, u32);
extern void InitVisualsCarScr(void);
extern void InitCarDynamicObjs(s16 slot, s16 car, s16 model);
extern s16 MBOX_FindObject_WORK(char *name, s8 lo, s8 hi, s32 err);
extern TexDef *MBOX_FindTexture_WORK(char *name, s16 *index, s8 lo, s8 hi, s32 err);

void InitVisuals(s32 for_game) {
    s32 i, j, k, model;
    u16 tex;
    char partName[16];

    for (i = 0; i < 32; i++) {
        paint_colors[i] = (u16)GPACK_RGBA5551(paint_colors_rgb[i].r, paint_colors_rgb[i].g, paint_colors_rgb[i].b, 0) | 1;
    }
    gCarTeamColor.a = 11;
    gCarTeamColor.b = 5;
    gCarTeamColor.c = 26;
    gCarTeamColor.d = 16;
    if (!for_game) {
        return;
    }
    memset(gCarParts, 0, 3328);
    if (gGameState & 0x100) {
        InitVisualsCarScr();
        for (j = 0; j < 156; j++) {
            gObjList[j] = 0;
        }
    } else {
        for (i = 0; i < 13; i++) {
            InitCarDynamicObjs(i, PC_LINK_CAR(i), gCarModelNums[i]);
            gCarPaintJobInited[i] = 0;
        }
    }
    for (k = 0; k < 24; k++) {
        if (MBOX_FindTexture_WORK(RimNames[k], (s16 *)&tex, 0, gNumModels - 1, 0)) {
            gWheelTextures[k] = &gTexDefPtrs[tex >> 10].defs[tex & 0x3FF];
        } else {
            gWheelTextures[k] = 0;
        }
    }
    for (k = 156; k != 157; k++) {
        gObjList[k] = MBOX_FindObject_WORK(tire_object_names[k - 156], 0, gNumModels - 1, 1);
    }
    for (k = 0; k < 10; k++) {
        MBOX_FindTexture_WORK(TexNames[k], &gTexList[k], 0, gNumModels - 1, 0);
    }
}
