/* InitTextureAnimation.c: set up a track's texture animations (MAT3 textureanim.c, InitTextureAnimation()).
 *
 * Each track has a list of palette animations (PaletteLists: palette rotations, light cycles, texture scrolls) and
 * a list of texture flipbooks (TextureSwapLists: a texture whose image steps through a series of others).
 * This looks their textures and palettes up by name once the track's models are loaded; TextureAnimation runs them
 * each frame. */
#include "game/game.h"

/* PC: Resource32 is a WAD's texture (TXHD: texBase at +24) or palette record (PLHD: palBase at +20), which keep
 * their N64 layout: the pointers are 32-bit RDRAM-window addresses (port/assets/wad.c). */
typedef struct Resource32 {
    u8 opaque[20];
    u32 palBase;        /* +20 a palette's colours */
    u32 texBase;        /* +24 a texture's load display list (TXLD), or its image (flags & TEX_IMAGE_DIRECT) */
    u32 flags;          /* +28 */
} Resource32;

/* A palette animation (MAT3 tPaletteAnim); state is a PALANIM_* (scene.h). */
typedef struct tPaletteAnim {
    char *name;         /* the palette's name (a texture's for the scrolls); NULL ends the list */
    s16 start, end;
    s8 offset;
    s8 state;
    s16 speed;          /* the period, in 30 Hz frames */
    f32 time;           /* the time left to the next step, seconds */
    void *ptr;          /* the palette's colours, or the texture's load display list */
} tPaletteAnim;

/* A texture flipbook (MAT3 tTextureSwap): the texture swapTexs[startnum] shows swapTexs[idx]'s image, idx stepping
 * every initTime seconds. */
typedef struct tTextureSwap {
    s16 num;            /* the frames; 0 ends the list */
    s16 startnum;       /* the frame whose texture is the one drawn */
    s16 fd;             /* 1 steps forward, 0 backward */
    s16 idx;            /* the frame shown */
    f32 time, initTime;
    Item12 *swapTexs;   /* the frames (MAT3 tTextureBase: name, tex, texBase) */
} tTextureSwap;

VIEW(tPaletteAnim *[], PaletteLists);
#define PaletteLists PaletteLists_view
VIEW(tTextureSwap *[], TextureSwapLists);
#define TextureSwapLists TextureSwapLists_view
VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

/* Find the textures and palettes of the track's (gTrackNum) animations, none in the attract demo without the
 * Expansion Pak. Palette animations: each restarts its period, and gets its palette's colours (or for a scroll, the
 * texture's load display list). Flipbooks (not when the race restarts: they are found already): each frame's
 * texture and its image address, the flipbook back at its first frame. Without the Expansion Pak the first six
 * tracks leave out the flipbooks that use the shared frames (texture_swap_shared_frames). */
void InitTextureAnimation(void) {
    tPaletteAnim *plist;
    tTextureSwap *tlist;
    Resource32 *resource;
    int i;
    u16 index;
    int unused1;
    int unused2;

    if ((gGameState & GS_DEMO) && !expansion_pak_present) return;
    plist = PaletteLists[gTrackNum];
    if (!plist) goto flipbooks;
    while (plist->name) {
        plist->time = 0.0333333f;
        if (plist->state >= PALANIM_SCROLL_S) plist->ptr = ((void *)PC_N64P(((Resource32 *) MBOX_FindTexture_WORK(plist->name, &index, 0, gNumModels - 1, 1))->texBase));
        else {
            resource = MBOX_FindPalette_WORK(plist->name, &index, 0, gNumModels - 1);
            if (resource) plist->ptr = ((void *)PC_N64P(resource->palBase));
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
                tlist->swapTexs[i].tex = MBOX_FindTexture_WORK(tlist->swapTexs[i].name, &index, 0, gNumModels - 1, 1);
            } else if (!expansion_pak_present && gTrackNum >= 0 && gTrackNum < 6 && tlist->swapTexs != texture_swap_shared_frames) {
                tlist->swapTexs[i].tex = MBOX_FindTexture_WORK(tlist->swapTexs[i].name, &index, 0, gNumModels - 1, 1);
            } else continue;
            resource = tlist->swapTexs[i].tex;
            if (resource->flags & TEX_IMAGE_DIRECT) tlist->swapTexs[i].texBase = resource->texBase;
            else tlist->swapTexs[i].texBase = displaylist_get_texture_image(((void *)PC_N64P(resource->texBase)));
            tlist->time = tlist->initTime;
            tlist->idx = tlist->startnum;
        }
        tlist++;
    }
}
