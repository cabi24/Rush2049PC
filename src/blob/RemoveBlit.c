#include "game/game.h"

/* PC: the Blit's and the sprite's host layouts (NewMultiBlit.c, particle_knot's Sprite), as every view has them
 * (docs/port/POINTERS.md 9). */
typedef struct Voice { void *name, *image, *info; u16 texIndex; short x, y; u16 z; short width, height;
    u8 alpha, flip, hide, init; short top, bot, left, right, color; u16 reserved; void *animFunc; unsigned int animID;
    void *animDTA; u16 slot; u16 reserved36; void *data; struct Voice *next; } Voice;
typedef struct PadConfig { void *pal, *tex; u16 model; short x, y; u16 mode; short w, h; u8 color, flags; u8 state;
    u8 pad17; short t0, s0, t1, s1; } PadConfig;
VIEW(PadConfig[], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view
VIEW(Voice *[], BlitPtrs);
#define BlitPtrs BlitPtrs_view

void RemoveBlit(Voice *voice) {
    s32 i;
    s32 s;

    while (voice != 0) {
        for (i = 0; i < NumBlits; i++) {
            if (voice == BlitPtrs[i]) break;
        }
        if (i) {}
        s = voice->slot;
        gBlitInstanceList[s].state = 2;
        /* Original game: nothing checks that the blit was found. Removing one that is not in BlitPtrs (a double
         * remove) leaves i == NumBlits and still takes one off NumBlits and swaps BlitPtrs[NumBlits] with the
         * entry past the end, which drops a live blit from the table (MAT3 blit.c alike). Kept as retail. */
        NumBlits--;
        BlitPtrs[i] = BlitPtrs[NumBlits];
        BlitPtrs[NumBlits] = voice;
        voice = voice->next;
    }
}
