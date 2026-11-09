/* DecodeVlist: unpacks a surface's list of vertices (MAT3 stree.c). */
#include "game/game.h"

/* Unpacks `vnum` vertex numbers from the packed list at `vlp` into `vlnum`: each entry is a big-endian u16, then,
 * if a byte of 0xE0 or more follows (and more than one vertex is left), its low 5 bits are how many vertices follow
 * on in sequence. Returns the u16 after the list (the surface list's start). */
s32 DecodeVlist(u8 *vlp, s32 vnum, s16 *vlnum)
{
    u32 next;
    s32 cnt;
    s32 t;
    if (vnum > 0) {
        do {
            next = (vlp[0] << 8) + vlp[1];
            vlp += 2;
            if ((vnum >= 2) && ((t = *vlp), (t >= 0xE0))) {
                cnt = t & ~0xE0;
                vlp += 1;
            } else {
                cnt = 0;
            }
            vnum = (vnum - cnt) - 1;
            do {
                *vlnum = next;
                cnt -= 1;
                next += 1;
                vlnum += 1;
            } while (cnt >= 0);
        } while (vnum > 0);
    }
    next = (vlp[0] << 8) + vlp[1];
    return next & 0xFFFF;
}
