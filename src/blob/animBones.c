/* animBones: the crossbones of deaths mode, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

/* Shows the crossbones of player animID by its speedometer while it races in deaths mode (bonesPos for the number of
 * windows); hides them otherwise (not deaths mode, no such player, or it finished). Returns whether hidden, 1
 * (the hide flag read as a signed byte, as the original reads it here). */
s32 animBones(void *blit)
{
    tBlit *blt = blit;
    u32 player;

    if (((s8) gDeathMode == 0) || ((player = blt->animID) >= (u32) (s16) gNumWindows) || (PC_CAR_PLACE_LOCKED(pc_models[player].slot) == 1)) {
        if (blt->hide != 1) {
            blt->hide = 1;
            UpdateBlit(blt);
        }
        return (s8)blt->hide;
    }
    blt->x = (s16) (bonesPos[(s16) gNumWindows - 1][player].x - (blt->width / 2));
    blt->y = (s16) bonesPos[(s16) gNumWindows - 1][player].y;
    UpdateBlit(blt);
    return 1;
}

