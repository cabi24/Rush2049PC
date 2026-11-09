/* NewBlit: make a blit (MAT3 blit.c). */
#include "game/game.h"

/* Takes a free blit from the pool and makes it the texture name (InitBlit: NULL a plain rectangle, BLIT_NO_TEXTURE a
 * text blit) at screen position x, y, shown, not flipped, without an animation function, with an entry of its own in
 * the zoid blit list. Returns NULL when all MAX_BLITS are in use. flags is unused here (NewMultiBlit passes bit 31 of
 * its animid; MAT3 halves the position when it is set). */
tBlit *NewBlit(char *name, s32 x, s32 y, s32 flags)
{
    tBlit *blit;

    if (NumBlits >= MAX_BLITS) {
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
    blit->blidx = ZOID_AddBlit(blit->texIndex, 0, 0, x, y, -1, -1);
    UpdateBlit(blit);
    return blit;
}
