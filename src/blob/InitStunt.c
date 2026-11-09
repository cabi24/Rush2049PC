/* InitStunt: the stunt records at a race's start (MAT3 stunt.c). */
#include "game/game.h"

extern void *memset(void *s, s32 c, u32 n);

VIEW(s32, gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view

VIEW(s32, gStuntInfo);
#define gStuntInfo gStuntInfo_view

/* Clears the four players' stunt scores (gStuntPlayer: MAT3 tStuntPlayer, 0x78 bytes each) and their stunts in
 * progress (gStuntInfo: MAT3 tStuntInfo, 0x7C bytes each, the N64's inline ResetStunts: each one's active word
 * gets 0x15000000), and the stunt clock.
 * PC: the walk over gStuntInfo stops at its end, gStuntInfo + 4. IDO took the address of the next variable for it,
 * last_frame_dl_end; on the PC that Gfx * has storage of its own (port/data), so a loop to it never met it and
 * cleared the arena to the window's end (docs/port/SYNTH.md 3). */
void InitStunt(void)
{
    s8 *p;
    memset(&gStuntPlayer, 0, 0x1E0U);
    do {
        p = (s8 *) &gStuntInfo;
        do {
            memset(p, 0, 0x7CU);
            *((s32 *) p) |= 0x15000000;
            p += 0x7C;
        } while (p != (s8 *) &gStuntInfo + 4 * 0x7C);
    } while (0);
    stunt_eTime = 0.0f;
    gStuntsDone = 0;
}
