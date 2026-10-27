/* port/assets/road.c: road / collision, waves 139..157 (docs/port/ASSETS.md §3.5).
 *
 * A 0x10-byte header (u16 pnum, tnum, snum, vnum, savenum, vlnum; u32 slnum), then the sections back to back:
 *   path   pnum x 0x84     33 f32
 *   tree   tnum x 0x14     s16 parent; u8 unused, flags; s16 minh, maxh, minv, maxv; u16 child[4]
 *   surf   snum x 0x18     12 x 16-bit (u16 flags, snd_flags; s16 uvs[9]; u16 vli)
 *   svert  vnum x 8        s16 pos[3]; u16 overflow
 *   save   savenum x 0x20  16 x 16-bit (u16 object, snum; s16 uvs[9]; u16 vnum; s16 pos[3]; u16 overflow)
 *   vlist  vlnum bytes, then slist: bytes, read byte by byte (two-byte values are assembled big-endian)
 * No pointers: InitSTree (func_800AB638) keeps the section addresses in game BSS. Every reader uses these widths
 * (checked for the 20 functions that read them), so this is a pure in-place swap. */
#include <stdio.h>

#include "assets.h"

static const size_t k_record[5] = {0x84, 0x14, 0x18, 8, 0x20};   /* path, tree, surf, svert, save */

void pc_assets_road(int wave, uint8_t *buf, size_t size)
{
    size_t pos = 0x10, i, s, n;

    if (size < 0x10)
        return;
    for (s = 0; s < 5; s++) {   /* everything must fit before anything is swapped */
        n = pc_be16(buf + 2 * s);
        if (n > (size - pos) / k_record[s]) {
            fprintf(stderr, "rush2049pc: road wave %d: sections run past the 0x%zx-byte block, left as is\n",
                    wave, size);
            return;
        }
        pos += n * k_record[s];
    }

    pc_swap16_n(buf, 6);
    pc_swap32_at(buf + 12);
    pos = 0x10;
    for (s = 0; s < 5; s++) {
        uint16_t c;
        memcpy(&c, buf + 2 * s, 2);
        n = c;
        if (s == 0)
            pc_swap32_n(buf + pos, n * 33);
        else if (s == 1)
            for (i = 0; i < n; i++) {
                pc_swap16_at(buf + pos + 0x14 * i);           /* parent; bytes 2 and 3 are u8 */
                pc_swap16_n(buf + pos + 0x14 * i + 4, 8);     /* minh, maxh, minv, maxv, child[4] */
            }
        else
            pc_swap16_n(buf + pos, n * k_record[s] / 2);      /* surf, svert, save: all 16-bit */
        pos += n * k_record[s];
    }
}
