#include "game/game.h"

typedef struct Item { s32 type, handle; f32 angle, matrix[3][3], position[3]; u8 alpha, pad[3]; } Item;
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Node { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 color; u8 tail[4]; } Node;

VIEW(Item[4][17], controls_buttonInfo);
#define controls_buttonInfo controls_buttonInfo_view

VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(Node[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

s32 ZOID_AddObject(s32 name, void *mat, s16 parent, s32 flags);

typedef struct NameEntry NameEntry;
NameEntry *MBOX_FindTexture_WORK(char *name, s16 *out, s8 lo, s8 hi, s32 err);
void ZOID_SetTexture(s16 handle, intptr_t tex, s32 c);

void controls_initButtons(s8 player) {
    s32 color;
    s32 k;
    s16 tex;
    s32 u0;
    s32 u1;
    s32 u2;
    s32 i;
    s32 mask;
    s16 none;
    Item *e;
    s32 h;
    f32 scale;

    color = PC_RGBA(controls_initButtonColor);
    scale = 1.0f;
    none = -1;
    if (player < 0) {}
    for (i = 0; i < 17; i++) {
        e = &controls_buttonInfo[player][i];
        if (e->handle == none) {
            mask = 1 << player;
            CopyUV(unitmatrix, e->matrix);
            h = ZOID_AddObject(MBOX_FindObject_WORK(controls_buttonNames[e->type], 0, (s8)(gNumModels - 1), 1), e->matrix, none,
                                      (e->type == 0 || e->type == 1 || e->type == 2 || e->type == 9 || e->type == 10) ? 0x42000 : 0);
            e->handle = h;
            ObjInstanceList[h].color = color;
            MBOX_HideObject(h, 1, 15);
            MBOX_ShowObject(e->handle, 0, mask);
        }
    }
    ZOID_SetTexture(controls_buttonInfo[player][13].handle, (intptr_t)MBOX_FindTexture_WORK("BUTTON_TITLE", &tex, 0, (s8)(gNumModels - 1), 1), -1);
    CopyUV(unitmatrix, controls_buttonInfo[player][13].matrix);
    WPitchUV(-1.5707964f, controls_buttonInfo[player][13].matrix);
    controls_buttonInfo[player][13].position[0] = 0.0f;
    if (gNumWindows >= 2)
        controls_buttonInfo[player][13].position[1] = 105.0f;
    else
        controls_buttonInfo[player][13].position[1] = 115.0f;
    controls_buttonInfo[player][13].position[2] = 200.0f;
    controls_buttonInfo[player][13].position[0] *= scale;
    controls_buttonInfo[player][13].position[1] *= scale;
    controls_buttonInfo[player][13].position[2] *= scale;
    ZOID_SetTexture(controls_buttonInfo[player][14].handle, (intptr_t)MBOX_FindTexture_WORK("CURSOR", &tex, 0, (s8)(gNumModels - 1), 1), -1);
    ZOID_SetTexture(controls_buttonInfo[player][15].handle, (intptr_t)MBOX_FindTexture_WORK("B1", &tex, 0, (s8)(gNumModels - 1), 1), -1);
    CopyUV(unitmatrix, controls_buttonInfo[player][15].matrix);
    WPitchUV(-1.5707964f, controls_buttonInfo[player][15].matrix);
    controls_buttonInfo[player][15].position[0] = -75.1f;
    if (gNumWindows >= 2)
        controls_buttonInfo[player][15].position[1] = 50.2f;
    else
        controls_buttonInfo[player][15].position[1] = 55.2f;
    controls_buttonInfo[player][15].position[2] = 100.0f;
    controls_buttonInfo[player][15].position[0] *= scale;
    controls_buttonInfo[player][15].position[1] *= scale;
    controls_buttonInfo[player][15].position[2] *= scale;
    ScaleUV(controls_buttonInfo[player][15].matrix, controls_buttonInfo[player][15].matrix, 0.5f);
    CopyUV(unitmatrix, controls_buttonInfo[player][16].matrix);
    WPitchUV(-1.5707964f, controls_buttonInfo[player][16].matrix);
    controls_buttonInfo[player][16].position[0] = -87.1f;
    if (gNumWindows >= 2)
        controls_buttonInfo[player][16].position[1] = 50.2f;
    else
        controls_buttonInfo[player][16].position[1] = 55.2f;
    controls_buttonInfo[player][16].position[2] = 100.0f;
    controls_buttonInfo[player][16].position[0] *= scale;
    controls_buttonInfo[player][16].position[1] *= scale;
    controls_buttonInfo[player][16].position[2] *= scale;
    controls_buttonsInited[player] = 1;
    controls_buttonsJustInited[player] = 1;
    ZeroCameras();
}

/* note: controls_initButtons = initButtons@controlscr.c (GC 0x8006CE5C; the N64 label is wrong).
 * body via, unchanged; matches with its real caller updateControlScreen in this group. The unused
 * locals k/u0/u1/u2 above remain the disclosed frame residual (112 vs 96). */
