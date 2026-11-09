#include "game/game.h"

/* PC: a pane group (FiveParts, InitPane.c) keeps its five blits pointer-sized from +0x10, and a Blit has its
 * host layout (NewMultiBlit.c): the palette (Image) at +8, Alpha at +0x24. The m2c body uses the N64 offsets. The
 * middle part takes the first colour, the frame parts the second. */
typedef struct { s8 active; u8 own; u8 rgba1[4]; u8 rgba2[4]; u8 other10[2]; void *part[5]; } PcFiveParts;
typedef struct PcBlit { const char *Name; void *Image; void *Info; u16 TexIndex; s16 X, Y; u16 Z; s16 Width, Height;
    u8 Alpha, Flip; s8 Hide, Init; s16 Top, Bot, Left, Right, color; u16 reserved; s32 (*AnimFunc)(struct PcBlit *);
    u32 AnimID; intptr_t AnimDTA; u16 BLIdx, reserved36; void *data; struct PcBlit *child; } PcBlit;
void ChangePaneColor(void *arg0, void *arg1, void *arg2) {
    PcFiveParts *group = (PcFiveParts *) arg0;
    u8 *c1 = (u8 *) arg1;
    u8 *c2 = (u8 *) arg2;
    PcBlit *part;
    s32 i;

    for (i = 0; i < 4; i++) {
        group->rgba1[i] = c1[i];
        group->rgba2[i] = c2[i];
    }
    for (i = 0; i < 5; i++) {
        part = (PcBlit *) group->part[i];
        if (i == 2) {
            part->Image = group->rgba1;
            part->Alpha = c1[3];
        } else {
            part->Image = group->rgba2;
            part->Alpha = c2[3];
        }
        UpdateBlit(part);
    }
}
