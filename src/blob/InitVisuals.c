#include "game/game.h"


#define GPACK_RGBA5551(r, g, b, a) ((((r) << 8) & 0xf800) | (((g) << 3) & 0x7c0) | (((b) >> 2) & 0x3e) | ((a) & 0x1))

typedef struct TexDef { u8 pad[36]; } TexDef;
typedef struct TexTable { TexDef *defs; u32 pad; } TexTable;

typedef struct Rgb { u8 r, g, b, a; } Rgb;
typedef struct Four { u8 a, b, c, d; } Four;

VIEW(Rgb[32], paint_colors_rgb);
#define paint_colors_rgb paint_colors_rgb_view
VIEW(u32[32], paint_colors);
#define paint_colors paint_colors_view
VIEW(Four, gCarTeamColor);
#define gCarTeamColor gCarTeamColor_view
VIEW(u8[], gCarParts);
#define gCarParts gCarParts_view

VIEW(s16[], gObjList);
#define gObjList gObjList_view

VIEW(char *[24], RimNames);
#define RimNames RimNames_view
VIEW(TexDef *[24], gWheelTextures);
#define gWheelTextures gWheelTextures_view
VIEW(TexTable[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(char *[10], TexNames);
#define TexNames TexNames_view
VIEW(s16[10], gTexList);
#define gTexList gTexList_view

extern void *memset(void *, s32, u32);

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
