#include "game/game.h"

VIEW(u8, gDeathMode);
#define gDeathMode gDeathMode_view

VIEW(u16, gNumWindows);
#define gNumWindows gNumWindows_view

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

/* referenced data (types guessed from access width) */
/* PC: the crossbones' positions, {x, y} per window count and player: bonesPos[4][4][2] of the GC's hud.c. The
 * complete declaration gives the table its extent (an M2C_UNK described only its first word to the loader). */
typedef struct { s32 x, y; } BonesPos;
VIEW(BonesPos[4][4], bonesPos);
#define bonesPos bonesPos_view
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

/* PC: animBones (GC hud.c), the crossbones by the speedometer in deaths mode, over the Blit's host layout
 * (NewMultiBlit.c's, docs/port/POINTERS.md 9.3): the name, image and info are pointers, so X..Hide are 0x0C later
 * than on the N64 and the AnimID (the player) is at +0x40. The N64 offsets of the m2c body below hid nothing on the
 * PC: outside deaths mode they set the low byte of X to 1 instead of Hide, and the skull was drawn at the top left. */
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

s32 animBones(void *arg0) {
    BonesBlit *blt = arg0;
    u32 player;

    if (((s8) gDeathMode == 0) || ((player = blt->AnimID) >= (u32) (s16) gNumWindows) || (PC_CAR_FINISHED(PC_MODEL_SLOT(player)) == 1)) {
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

