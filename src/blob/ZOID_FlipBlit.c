/* ZOID_FlipBlit: mirror a zoid blit list entry (MAT3 zoidblit.c). */
#include "game/game.h"

VIEW(tBlitInstance[4], gBlitInstanceList);
#define gBlitInstanceList (gBlitInstanceList_view[0])

/* Sets or clears BLITINST_FLIP of entry index: the renderer then draws it mirrored left to right. */
void ZOID_FlipBlit(s32 index, s32 flip) {
    tBlitInstance *inst;
    tBlitInstance *inst2;

    if (flip != 0) {
        inst = &(&gBlitInstanceList)[index];
        inst->flags |= BLITINST_FLIP;
        return;
    }
    inst2 = &(&gBlitInstanceList)[index];
    inst2->flags &= 0xFFFB;   /* ~BLITINST_FLIP */
}
