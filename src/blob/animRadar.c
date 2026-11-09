/* animRadar: the radar's frame, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

typedef struct { s32 x, y; } Pos;

VIEW(s32, gGameState);
#define gGameState gGameState_view
VIEW(Pos[][4], radarPos);
#define radarPos radarPos_view

/* The radar of player animID: hidden (and its animation stopped) past the windows, in attract mode or with fewer than
 * two cars; hidden when the HUD hides it or the car finished; else placed at radarPos, the medium texture and alpha
 * 0x60 on split screens. Its size goes to radarWidth and radarHeight for the blips. Returns whether hidden, 1. */
s32 animRadar(tBlit *node) {
    s32 index;
    s32 hidden;

    index = node->animID;
    if (index >= gNumWindows || (gGameState & GS_DEMO) || gNumCars < 2) {
        node->animFunc = 0;
        if (node->hide != 1) {
            node->hide = 1;
            UpdateBlit(node);
        }
        return node->hide;
    }
    hidden = gShowRadar == 0;
    if (hidden == 0) {
        hidden = PC_CAR_FINISHED(index) == 1;
    }
    if (hidden != node->hide) {
        node->hide = hidden;
        UpdateBlit(node);
    }
    if (node->hide != 0) {
        return 1;
    }
    node->x = radarPos[gNumWindows - 1][index].x;
    node->y = radarPos[gNumWindows - 1][index].y;
    if (gNumWindows >= 2) {
        RenameBlit(node, radar_md_blit_name, 0);
        node->alpha = 0x60;
    }
    UpdateBlit(node);
    radarWidth = node->width;
    radarHeight = node->height;
    return 1;
}
