/* animBones: the crossbones of deaths mode, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

/* The blit as this reads it (tBlit's layout; the animation function's slot read as an integer). */
typedef struct BonesBlit {
    void *Name, *Image, *Info;
    u16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide, Init;
    s16 Top, Bot, Left, Right, color;
    u16 reserved;
    intptr_t AnimDTA;   /* the AnimFunc slot */
    u32 AnimID;
} BonesBlit;

/* Shows the crossbones of player animID by its speedometer while it races in deaths mode (bonesPos for the number of
 * windows); hides them otherwise (not deaths mode, no such player, or it finished). Returns whether hidden, 1. */
s32 animBones(void *blit) {
    BonesBlit *blt = blit;
    u32 player;

    if (((s8) gDeathMode == 0) || ((player = blt->AnimID) >= (u32) (s16) gNumWindows) || (PC_CAR_PLACE_LOCKED(PC_MODEL_SLOT(player)) == 1)) {
        if (blt->Hide != 1) {
            blt->Hide = 1;
            UpdateBlit(blt);
        }
        return blt->Hide;
    }
    blt->X = (s16) (bonesPos[(s16) gNumWindows - 1][player].x - (blt->Width / 2));
    blt->Y = (s16) bonesPos[(s16) gNumWindows - 1][player].y;
    UpdateBlit(blt);
    return 1;
}

