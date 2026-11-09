/* controls_initButtons: the controller-setup screen's 3D buttons for one player (MAT3 controlscr.c initButtons). */
#include "game/game.h"

VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

s32 ZOID_AddObject(s32 name, void *mat, s16 parent, s32 flags);

typedef struct NameEntry NameEntry;
NameEntry *MBOX_FindTexture_WORK(char *name, s16 *out, s8 lo, s8 hi, s32 err);
void ZOID_SetTexture(s16 handle, intptr_t tex, s32 c);

/* Make player's 17 buttons that don't exist yet, each a model by its type (controls_buttonNames; types 0, 1, 2, 9
 * and 10 draw see-through, 0x42000), white, shown only in that player's window. Then texture and place the title,
 * cursor and B icon, the title and icon a little lower with more than one window. */
void controls_initButtons(s8 player) {
    s32 color;
    s16 tex;
    s32 i;
    s32 mask;
    s16 none;
    MenuButton *e;
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
    ZOID_SetTexture(controls_buttonInfo[player][CONTROLS_TITLE].handle, (intptr_t)MBOX_FindTexture_WORK("BUTTON_TITLE", &tex, 0, (s8)(gNumModels - 1), 1), -1);
    CopyUV(unitmatrix, controls_buttonInfo[player][CONTROLS_TITLE].matrix);
    WPitchUV(-1.5707964f, controls_buttonInfo[player][CONTROLS_TITLE].matrix);
    controls_buttonInfo[player][CONTROLS_TITLE].position[0] = 0.0f;
    if (gNumWindows >= 2)
        controls_buttonInfo[player][CONTROLS_TITLE].position[1] = 105.0f;
    else
        controls_buttonInfo[player][CONTROLS_TITLE].position[1] = 115.0f;
    controls_buttonInfo[player][CONTROLS_TITLE].position[2] = 200.0f;
    controls_buttonInfo[player][CONTROLS_TITLE].position[0] *= scale;
    controls_buttonInfo[player][CONTROLS_TITLE].position[1] *= scale;
    controls_buttonInfo[player][CONTROLS_TITLE].position[2] *= scale;
    ZOID_SetTexture(controls_buttonInfo[player][CONTROLS_CURSOR].handle, (intptr_t)MBOX_FindTexture_WORK("CURSOR", &tex, 0, (s8)(gNumModels - 1), 1), -1);
    ZOID_SetTexture(controls_buttonInfo[player][CONTROLS_B_ICON].handle, (intptr_t)MBOX_FindTexture_WORK("B1", &tex, 0, (s8)(gNumModels - 1), 1), -1);
    CopyUV(unitmatrix, controls_buttonInfo[player][CONTROLS_B_ICON].matrix);
    WPitchUV(-1.5707964f, controls_buttonInfo[player][CONTROLS_B_ICON].matrix);
    controls_buttonInfo[player][CONTROLS_B_ICON].position[0] = -75.1f;
    if (gNumWindows >= 2)
        controls_buttonInfo[player][CONTROLS_B_ICON].position[1] = 50.2f;
    else
        controls_buttonInfo[player][CONTROLS_B_ICON].position[1] = 55.2f;
    controls_buttonInfo[player][CONTROLS_B_ICON].position[2] = 100.0f;
    controls_buttonInfo[player][CONTROLS_B_ICON].position[0] *= scale;
    controls_buttonInfo[player][CONTROLS_B_ICON].position[1] *= scale;
    controls_buttonInfo[player][CONTROLS_B_ICON].position[2] *= scale;
    ScaleUV(controls_buttonInfo[player][CONTROLS_B_ICON].matrix, controls_buttonInfo[player][CONTROLS_B_ICON].matrix, 0.5f);
    CopyUV(unitmatrix, controls_buttonInfo[player][CONTROLS_B_ICON_BACK].matrix);
    WPitchUV(-1.5707964f, controls_buttonInfo[player][CONTROLS_B_ICON_BACK].matrix);
    controls_buttonInfo[player][CONTROLS_B_ICON_BACK].position[0] = -87.1f;
    if (gNumWindows >= 2)
        controls_buttonInfo[player][CONTROLS_B_ICON_BACK].position[1] = 50.2f;
    else
        controls_buttonInfo[player][CONTROLS_B_ICON_BACK].position[1] = 55.2f;
    controls_buttonInfo[player][CONTROLS_B_ICON_BACK].position[2] = 100.0f;
    controls_buttonInfo[player][CONTROLS_B_ICON_BACK].position[0] *= scale;
    controls_buttonInfo[player][CONTROLS_B_ICON_BACK].position[1] *= scale;
    controls_buttonInfo[player][CONTROLS_B_ICON_BACK].position[2] *= scale;
    controls_buttonsInited[player] = 1;
    controls_buttonsJustInited[player] = 1;
    ZeroCameras();
}
