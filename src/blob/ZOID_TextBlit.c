/* ZOID_TextBlit: mark a zoid blit list entry as a text blit (MAT3 zoidblit.c). */
#include "game/game.h"

VIEW(tBlitInstance[4], gBlitInstanceList);
#define gBlitInstanceList (gBlitInstanceList_view[0])

/* Sets or clears BLITINST_TEXT of entry index: a text blit has no texture, and the renderer calls the function in its
 * texture field with its palette field (the blit) to draw it. */
void ZOID_TextBlit(s32 index, s32 text) {
    tBlitInstance *inst;
    tBlitInstance *inst2;

    if (text != 0) {
        inst = &(&gBlitInstanceList)[index];
        inst->flags |= BLITINST_TEXT;
        return;
    }
    inst2 = &(&gBlitInstanceList)[index];
    inst2->flags &= 0xFF7F;   /* ~BLITINST_TEXT */
}
