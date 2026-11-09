/* animHealth: a battle car's health bar, a blit animation function (MAT3 hudextra.c; runtime image B). */
#include "game/game.h"

#define HEALTH_FULL 800.0f   /* a car's health at its best */
#define HEALTH_FRAME 0       /* animID's low nibble: the bar's frame; else the bar */


#define PAL_BASE(p) PC_N64W(p)

/* The health bar of player animID >> 4 at healthPos: the frame (drawn with the alpha threshold), or the bar, as long
 * as the health left, in a colour from green through yellow to red (written into the player's palette, healthPal).
 * Past the players it is hidden and stops animating. Returns 1. */
int animHealth(tBlit *blt)
{
    int slot = (blt->animID & 0xf0) >> 4;
    int type = blt->animID & 15;
    float healthPct;
    if (slot >= gNumPlayers) {
        blt->animFunc = 0;
        return Hidden(blt, 1);
    }
    healthPct = PC_CAR_HEALTH(slot) / HEALTH_FULL;
    blt->x = healthPos[gNumWindows-1][slot][0] - blt->width / 2;
    blt->y = healthPos[gNumWindows-1][slot][1] - blt->height / 2;
    if (type == HEALTH_FRAME) {
        { tBlitInstance *inst = &gBlitInstanceList[blt->blidx]; inst->flags |= BLITINST_ALPHA_COMPARE; }
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
