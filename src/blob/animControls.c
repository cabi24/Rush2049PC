/* animControls: the controller-setup screen's button icons (MAT3 controlscr.c animControls): the blit callback of
 * each row of the controls list, which shows the button or stick a control is mapped to. */
#include "game/game.h"

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    uintptr_t word0, word4, word8;
    u16 half12;
    s16 x, y;
    u16 half18;
    s16 w, h;
    u8 alpha, byte25;
    s8 hidden;
    u8 byte27;
    s16 left, right, top, bottom;
    u32 word36;
    intptr_t state;     /* AnimFunc */
    u32 data;
} Blit;

/* A player's pad record (MAT3 tPadState, gPlayers). */
typedef struct Player {
    u8 slot;
    u8 controller;          /* 0x01 its controller, CONTROLLER_NONE without one */
    u8 pad2[0x48 - 0x2]; void *ref;
} Player;

extern Player gPlayers[];

VIEW(u8[][152], gCamera);
#define gCamera gCamera_view

#define CONTROLLER_NONE 5           /* gPlayers[].controller of a player without one */
#define CONTROLS_COUNT 10           /* the controls a profile keeps */
#define CONTROL_ANALOG_STICK 25     /* a control's last value, an analog source: gas and brake (rows 0, 1) show icon 26 for it */

/* The controls list's rows as this file counts them: every one but row 8. */
static s32 validOption(u8 player, s32 item)
{
    s32 valid;

    valid = 1;
    if (item == 8) {
        valid = 0;
    }
    return valid;
}

/* The icon of one row of a player's controls list, each frame. The blit's data packs its row (bits 0-3, counted
 * from the first row shown), a conflict marker flag (4-7: this blit marks a conflict instead of showing an icon),
 * an "uninited" flag (8-11), the player (12-15) and the value shown plus one (16-23). Hidden when the player has no
 * controller or hasn't the screen open, past the controls or the rows shown, or (a conflict marker) when its row
 * has no conflict. The first frame places it (small icons with more than one window) beside its 3D button or text
 * row; each frame it follows its button and its alpha. The icon changes when the control's value does
 * (controls_idMap, the conflict marker controls_idMap_conflict). Always 1. */
s32 animControls(Blit *blt)
{
    s32 id;
    s32 conflict;
    s32 uninited;
    s32 p;
    s32 val;
    s32 action;
    s16 i;
    s16 j;
    s16 screenPos[2];
    f32 gTextPosScale;

    conflict = (blt->data >> 4) & 15;
    uninited = (blt->data >> 8) & 15;
    p = (blt->data >> 12) & 15;
    id = blt->data & 15;
    val = ((blt->data >> 16) & 255) - 1;
    action = gControlFirst[p] + id - 1;
    gTextPosScale = controls_gButtonScale;
    if (gNumWindows > 1) {
        gTextPosScale *= 0.5f;
    }
    for (i = 0, j = 0; i < id; j++) {
        if (!validOption(p, gControlFirst[p] + j)) {
            action++;
        } else {
            i++;
        }
    }
    if (Hidden(blt, gPlayers[p].controller == CONTROLLER_NONE || (conflict && !gConflict[p][action]) || action >= CONTROLS_COUNT ||
                        !controlScrActive[p] || gControlMenuSize < id)) {
        return 1;
    }
    if (uninited) {
        if (gNumWindows > 1) {
            RenameBlit(blt, "CONTROLS_SM", 0);
        }
        if (controls_buttonsInited[p]) {
            ZOID_ProjectWindow(p, controls_buttonInfo[p][action].position, gCamera[p], 0, screenPos);
            blt->x = screenPos[0] + 55.0f * gTextPosScale * 300.0f / controls_buttonZ;
            blt->y = screenPos[1] - blt->h / 2;
            blt->alpha = controls_buttonInfo[p][action].alpha;
        } else {
            blt->x = controls_gPlayerRects[gNumWindows - 1][p].cx + 20;
            blt->y = controls_gPlayerRects[gNumWindows - 1][p].y + id * 16 + 8;
        }
        if (conflict) {
            blt->x += (gNumWindows == 1) ? 20 : 10;
        }
        UpdateBlit(blt);
        blt->data &= 0x00FFF0FF;
        if (conflict) {
            blt->data = (blt->data & 0xFFFF) | 0x1C0000;
            SelectBlit(blt, controls_idMap_conflict, blt->h, blt->h);
        }
    }
    if (controls_buttonsInited[p]) {
        ZOID_ProjectWindow(p, controls_buttonInfo[p][action].position, gCamera[p], 0, screenPos);
        blt->x = screenPos[0] + 75.0f * gTextPosScale * 300.0f / controls_buttonZ;
        blt->y = screenPos[1] - blt->h / 2;
        blt->half18 = 0x7F00;
        if (conflict) {
            blt->x += (gNumWindows == 1) ? 20 : 10;
        }
        if (controls_buttonInfo[p][action].alpha >= 255) {
            blt->alpha = 254;
        } else {
            blt->alpha = controls_buttonInfo[p][action].alpha;
        }
        if (Hidden(blt, blt->alpha == 0)) {
            return 1;
        }
        UpdateBlit(blt);
    } else {
        blt->alpha = 254;
        UpdateBlit(blt);
    }
    if (conflict) {
        return 1;
    }
    if (val != gNewControls[gPlayers[p].controller][action]) {
        val = gNewControls[gPlayers[p].controller][action];
        blt->data = (blt->data & 0xFFFF) | (((val + 1) << 16) & 0xFF0000);
        if (val == CONTROL_ANALOG_STICK && (action == 0 || action == 1)) {
            val++;
        }
        SelectBlit(blt, controls_idMap[val], blt->h, blt->h);
    }
    return 1;
}
