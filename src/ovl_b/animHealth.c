#include "game/game.h"


#define PAL_BASE(p) PC_N64W(p)
typedef struct _tBlit_2 {
    const char *name;
    tPalette *palette;
    void *texture;
    u16 texIndex;
    s16 x, y;
    u16 z;
    s16 width, height;
    u8 alpha, flip;
    char hide, init;
    s16 top, bot, left, right, color;
    int (*animFunc)(struct _tBlit_2 *);
    u32 animID;
    intptr_t animDTA;
    u16 blidx;
    void *data;
    struct _tBlit_2 *child;
} _tBlit_2;

VIEW(s32[4][4][2], healthPos);
#define healthPos healthPos_view
VIEW(u8[4][2][4], healthColor);
#define healthColor healthColor_view
VIEW(tPalette[4], healthPal);
#define healthPal healthPal_view

int animHealth(_tBlit_2 *blt) /* animHealth */
{
    int slot = (blt->animID & 0xf0) >> 4;
    int type = blt->animID & 15;
    float healthPct;
    if (slot >= gNumPlayers) {
        blt->animFunc = 0;
        return Hidden(blt, 1);
    }
    healthPct = PC_CAR_HEALTH(slot) / 800.0f;
    blt->x = healthPos[gNumWindows-1][slot][0] - blt->width / 2;
    blt->y = healthPos[gNumWindows-1][slot][1] - blt->height / 2;
    if (type == 0) {
        { tBlitInstance *inst = &gBlitInstanceList[blt->blidx]; inst->flags |= 1; }
    } else {
        blt->x += 8;
        blt->left = 8;
        blt->right = 8.0f + (blt->width - 17) * healthPct;
        if (healthPct > 0.8f) {
            healthColor[slot][0][0] = 0; healthColor[slot][0][1] = 224; healthColor[slot][0][2] = 0;
            healthColor[slot][1][0] = 0; healthColor[slot][1][1] = 112; healthColor[slot][1][2] = 0;
        } else if (healthPct > 0.6f) {
            healthColor[slot][0][0] = 224.0f * (5.0f * (0.8f - healthPct));
            healthColor[slot][0][1] = 224;
            healthColor[slot][0][2] = 0;
            healthColor[slot][1][0] = 112.0f * (5.0f * (0.8f - healthPct));
            healthColor[slot][1][1] = 112;
            healthColor[slot][1][2] = 0;
        } else if (healthPct > 0.4f) {
            healthColor[slot][0][0] = 224; healthColor[slot][0][1] = 224; healthColor[slot][0][2] = 0;
            healthColor[slot][1][0] = 112; healthColor[slot][1][1] = 112; healthColor[slot][1][2] = 0;
        } else if (healthPct > 0.2f) {
            healthColor[slot][0][0] = 224;
            healthColor[slot][0][1] = 224.0f * (5.0f * (healthPct - 0.2f));
            healthColor[slot][0][2] = 0;
            healthColor[slot][1][0] = 112;
            healthColor[slot][1][1] = 112.0f * (5.0f * (healthPct - 0.2f));
            healthColor[slot][1][2] = 0;
        } else {
            healthColor[slot][0][0] = 224; healthColor[slot][0][1] = 0; healthColor[slot][0][2] = 0;
            healthColor[slot][1][0] = 112; healthColor[slot][1][1] = 0; healthColor[slot][1][2] = 0;
        }
        healthPal[slot].palBase = PAL_BASE((char *)healthColor[slot]);
        blt->palette = &healthPal[slot];
    }
    UpdateBlit(blt);
    return 1;
}
