/* DecodeSlist: unpacks a tree node's list of surfaces (MAT3 stree.c). */
#include "game/game.h"

/* Unpacks `snum` surface numbers from the packed list at `slp` into `slnum`: each entry is a big-endian u16 whose
 * top 3 bits are how many surfaces follow on in sequence and the rest the first. Surface `sli` goes to the front of
 * the list, the others fill it from the back, so the surface last found comes first. */
void DecodeSlist(u8 *slp, s32 snum, u16 *slnum, u32 sli)
{
    u16 *back = slnum + snum - 1;
    u32 next;
    s32 cnt;

    while (snum > 0) {
        next = ((u32)slp[0] << 8) + slp[1];
        slp += 2;
        cnt = next >> 13;
        next &= 0x1FFF;
        snum = snum - cnt - 1;
        do {
            cnt--;
            if (next == sli) {
                *slnum++ = next++;
            } else {
                *back-- = next++;
            }
        } while (cnt >= 0);
    }
}
