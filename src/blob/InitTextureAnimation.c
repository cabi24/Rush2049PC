/* InitTextureAnimation.c: set up a track's texture animations (MAT3 textureanim.c, InitTextureAnimation()).
 *
 * Each track has a list of palette animations (PaletteLists: palette rotations, light cycles, texture scrolls) and
 * a list of texture flipbooks (TextureSwapLists: a texture whose image steps through a series of others).
 * This looks their textures and palettes up by name once the track's models are loaded; TextureAnimation runs them
 * each frame. */
#include "game/game.h"

/* Find the textures and palettes of the track's (gTrackNum) animations, none in the attract demo without the
 * Expansion Pak. Palette animations: each restarts its period, and gets its palette's colours (or for a scroll, the
 * texture's load display list). Flipbooks (not when the race restarts: they are found already): each frame's
 * texture and its image address, the flipbook back at its first frame. Without the Expansion Pak the first six
 * tracks leave out the flipbooks that use the shared frames (texture_swap_shared_frames). */
void InitTextureAnimation(void) {
    tPaletteAnim *plist;
    tTextureSwap *tlist;
    tPaletteDef *palette;
    tTexture *texture;
    int i;
    u16 index;
    int unused1;
    int unused2;

    if ((gGameState & GS_DEMO) && !expansion_pak_present) return;
    plist = PaletteLists[gTrackNum];
    if (!plist) goto flipbooks;
    while (plist->name) {
        plist->time = 0.0333333f;
        if (plist->state >= PALANIM_SCROLL_S) plist->ptr.commands = MBOX_FindTexture_WORK(plist->name, &index, 0, gNumModels[0] - 1, 1)->texBase;
        else {
            palette = MBOX_FindPalette_WORK(plist->name, &index, 0, gNumModels[0] - 1);
            if (palette) plist->ptr.palette = palette->palBase;
        }
        plist++;
    }
flipbooks:
    if (gRestartGame) return;
    tlist = TextureSwapLists[gTrackNum];
    if (!tlist) return;
    while (tlist->num) {
        for (i = 0; i < tlist->num; i++) {
            if (expansion_pak_present || gTrackNum >= 6) {
                tlist->swapTexs[i].tex = MBOX_FindTexture_WORK(tlist->swapTexs[i].name, &index, 0, gNumModels[0] - 1, 1);
            } else if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6 && tlist->swapTexs != texture_swap_shared_frames) {
                tlist->swapTexs[i].tex = MBOX_FindTexture_WORK(tlist->swapTexs[i].name, &index, 0, gNumModels[0] - 1, 1);
            } else continue;
            texture = tlist->swapTexs[i].tex;
            /* the frame's image: a texture's own texels, or the image its load list's texture image record holds
             * (which TextureAnimation writes into the drawn texture's texBase or its own load list) */
            if (texture->flags & TEX_IMAGE_DIRECT) tlist->swapTexs[i].texBase = texture->texBase;
            else tlist->swapTexs[i].texBase = displaylist_get_texture_image(texture->texBase);
            tlist->time = tlist->initTime;
            tlist->idx = tlist->startnum;
        }
        tlist++;
    }
}
