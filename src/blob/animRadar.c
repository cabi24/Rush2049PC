#include "game/game.h"

/* PC: the node is a blit (NewMultiBlit.c's Blit, 0x68 bytes, docs/port/POINTERS.md 9.3): its name, image and info
 * are pointers, so the rest is 0x0C later, and the AnimFunc slot (word28) is pointer-sized at +0x38 with the AnimID
 * (the player index) after it at +0x40. */
typedef struct MenuNode {
    void *next;         /* 0x00 */
    void *image;        /* 0x08 */
    void *info;         /* 0x10 */
    s16 padC;           /* 0x18 */
    s16 x;              /* 0x1A */
    s16 y;              /* 0x1C */
    s16 pad12;          /* 0x1E */
    s16 w;              /* 0x20 */
    s16 h;              /* 0x22 */
    s8 alpha;           /* 0x24 */
    s8 pad19;
    s8 hidden;          /* 0x26 */
    s8 pad1B;
    s32 pad1C, pad20, pad24;
    intptr_t word28;    /* 0x38 */
    s32 index;          /* 0x40: player index */
} MenuNode;

typedef struct { s32 x, y; } Pos;

VIEW(s32, gGameState);
#define gGameState gGameState_view
VIEW(Pos[][4], radarPos);
#define radarPos radarPos_view

s32 animRadar(MenuNode *node) {
    s32 index;
    s32 hidden;

    index = node->index;
    if (index >= gNumWindows || (gGameState & 8) || gNumCars < 2) {
        node->word28 = 0;
        if (node->hidden != 1) {
            node->hidden = 1;
            UpdateBlit(node);
        }
        return node->hidden;
    }
    hidden = gShowRadar == 0;
    if (hidden == 0) {
        hidden = PC_CAR_FINISHED(index) == 1;
    }
    if (hidden != node->hidden) {
        node->hidden = hidden;
        UpdateBlit(node);
    }
    if (node->hidden != 0) {
        return 1;
    }
    node->x = radarPos[gNumWindows - 1][index].x;
    node->y = radarPos[gNumWindows - 1][index].y;
    if (gNumWindows >= 2) {
        RenameBlit(node, radar_md_blit_name, 0);
        node->alpha = 0x60;
    }
    UpdateBlit(node);
    radarWidth = node->w;
    radarHeight = node->h;
    return 1;
}
