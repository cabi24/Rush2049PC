/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * RemoveBlit(voice) -- release a linked list of voices (next at +0x3C). For each voice: find it in the
 * active pool BlitPtrs[0..NumBlits) (linear search, i = count when absent), mark its 32-byte
 * gBlitInstanceList[voice->slot] record's byte 22 = 2 (released), then swap-remove it: count--,
 * pool[i] = pool[count], pool[count] = voice. (Names are historical labels; N64 audio code, no arcade
 * ancestor identified.)
 *
 * Shaping (closes 's 12 rows after ~150 earlier variants):
 *  - the compiled-out `if (i) {}` right after the search loop is what puts the i*4 shift (`sll t7,v0,2`)
 *    first, in the exit path and the beql delay slot, before the slot load;
 *  - the slot is a named local `s` (a register web, lhu into v0 as in retail);
 * - no compiled-out pool check (`if (BlitPtrs[i] != voice) {}`): that check made
 *    &BlitPtrs the first address web, so phase 2 handed out a3/t0/t1/t2 in the wrong order.
 *    Without it the address webs are numbered gBlitInstanceList, 2, count, pool as in retail.
 * Also MATCH at -O2. Leaf, no frame. The caller-less stub blit_stub before it is left as locked.
 */
typedef unsigned char u8; typedef unsigned short u16; typedef int s32;
/* PC: the Blit's and the sprite's host layouts (NewMultiBlit.c, particle_knot's Sprite), as every view has them
 * (docs/port/POINTERS.md 9). */
typedef struct Voice { void *name, *image, *info; u16 texIndex; short x, y; u16 z; short width, height;
    u8 alpha, flip, hide, init; short top, bot, left, right, color; u16 reserved; void *animFunc; unsigned int animID;
    void *animDTA; u16 slot; u16 reserved36; void *data; struct Voice *next; } Voice;
typedef struct PadConfig { void *pal, *tex; u16 model; short x, y; u16 mode; short w, h; u8 color, flags; u8 state;
    u8 pad17; short t0, s0, t1, s1; } PadConfig;
extern PadConfig gBlitInstanceList[];
extern Voice *BlitPtrs[];
extern s32 NumBlits;

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
