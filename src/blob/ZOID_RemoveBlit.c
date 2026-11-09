/* ZOID_RemoveBlit: free an entry of the zoid blit list (MAT3 zoidblit.c). */
#include "game/game.h"

VIEW(tBlitInstance, gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view

/* Frees entry index (hide BLITINST_FREE): the renderer skips it and ZOID_AddBlit hands it out again. */
void ZOID_RemoveBlit(s32 index) {
    ((tBlitInstance *) &gBlitInstanceList)[index].hide = BLITINST_FREE;
}
