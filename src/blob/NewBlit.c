#include "game/game.h"

typedef struct Blit {
    /* 0x00 */ char *name;
    /* 0x04 */ void *image;
    /* 0x08 */ void *info;
    /* 0x0C */ u16 texIndex;
    /* 0x0E */ s16 x;
    /* 0x10 */ s16 y;
    /* 0x12 */ u16 state;
    /* 0x14 */ u16 width, height;
    /* 0x18 */ u8 alpha;
    /* 0x19 */ u8 flip;
    /* 0x1A */ u8 hide;
    /* 0x1B */ u8 init;
    /* 0x1C */ s16 top, bot, left, right, color;
    /* 0x26 */ s16 pad26;
    /* 0x28 */ void *animFunc;
    /* 0x2C */ unsigned int animID;
    /* 0x30 */ intptr_t animDTA;
    /* 0x34 */ s16 blIdx;
    /* 0x36 */ s16 pad36;
    /* 0x38 */ void *data;
    /* 0x3C */ struct Blit *child;
} Blit;

#define MAXBLITS 200

Blit *NewBlit(char *name, s32 x, s32 y, s32 flags)
{
    Blit *blit;

    if (NumBlits >= MAXBLITS) {
        return 0;
    }

    if (flags) {}
    blit = BlitPtrs[NumBlits];
    NumBlits++;

    blit->name = name;
    blit->x = x;
    blit->y = y;
    blit->hide = 0;
    blit->flip = 0;
    blit->init = 1;
    blit->child = 0;
    blit->animFunc = 0;
    blit->animID = -1;
    blit->animDTA = -1;
    blit->data = 0;
    InitBlit(blit);
    blit->blIdx = ZOID_AddBlit(blit->texIndex, 0, 0, x, y, -1, -1);
    UpdateBlit(blit);
    return blit;
}
