/* port/assets/path.c: paths, waves 158..182 (158..176 forward, 177..182 backward; docs/port/ASSETS.md §3.6).
 *
 * Layout, as ReadCheckpointData (audio_dsp_process) and ReadPathData (audio_effect_apply, groups/read_path_data)
 * walk it:
 *   0x000  checkpoints: s16 startTime, loopPt, finishPt, startPt, numPoints; 2 pad; 10 x 0x50
 *          {f32 pos[3], normal[3]; 32-bit radius; s16 flags; s16 time[2]; s16 pathPts[20]; 2 pad; f32 distToNext}
 *   0x32C  resurrection path, 0x10: u16 numPts; u16; u32 pts slot; u8 numAlt; 3 pad; u32 alt slot
 *   0x33C  numAlt x 0x10 alternative paths: u8 resurrect; s8 firstPath; u16 firstIdx; s8 lastPath; pad;
 *          u16 lastIdx; s8 startSection; pad; u16 numPts @0xA; u32 pts slot @0xC
 *          u16 resurrection point count; numPts x s16[3]; for each alternative path its numPts x s16[3]
 *          4 x {drone path 8 B: u16 num; u16; u32 pts slot; num x 8 B points {s16 pos[3]; u8 mph; s8 hints}}
 * Only 2-byte alignment is guaranteed after the alternative paths; the swaps are bytewise.
 *
 * The pointer slots are swapped like any u32 and never read: on PC ReadPathData copies the headers field by
 * field into native structs and keeps the alternative paths in a native array (ASSETS.md §4 #22). */
#include <stdio.h>

#include "assets.h"

#define CHECKPOINTS 0x32C

/* Walks the file, reading its counts big-endian. With swap set it also swaps every field it passes; without, it
 * only checks that everything fits. Returns 0 if the file runs past size. */
static int walk(uint8_t *buf, size_t size, int swap)
{
    size_t pos, i, alt_at;
    unsigned primary, alts, n;
    int p;

    if (size < CHECKPOINTS + 0x10)
        return 0;
    if (swap) {
        pc_swap16_n(buf, 6);   /* five s16 and the pad */
        for (i = 0; i < 10; i++) {
            uint8_t *c = buf + 12 + 0x50 * i;
            pc_swap32_n(c, 7);            /* pos[3], normal[3], radius */
            pc_swap16_n(c + 0x1C, 23);    /* flags, time[2], pathPts[20] */
            pc_swap32_at(c + 0x4C);       /* distToNext */
        }
    }

    pos = CHECKPOINTS;
    primary = pc_be16(buf + pos);
    alts = buf[pos + 8];
    if (swap) {
        pc_swap16_n(buf + pos, 2);
        pc_swap32_at(buf + pos + 4);
        pc_swap32_at(buf + pos + 12);
    }
    pos += 0x10;

    alt_at = pos;
    if ((size_t)alts * 0x10 > size - pos)
        return 0;
    pos += (size_t)alts * 0x10;

    if (size - pos < 2 || (size_t)primary * 6 > size - pos - 2)
        return 0;
    if (swap)
        pc_swap16_n(buf + pos, 1 + (size_t)primary * 3);   /* the point count, then the points */
    pos += 2 + (size_t)primary * 6;
    for (i = 0; i < alts; i++) {
        uint8_t *a = buf + alt_at + 0x10 * i;
        n = pc_be16(a + 0xA);
        if ((size_t)n * 6 > size - pos)
            return 0;
        if (swap) {
            pc_swap16_at(a + 2);
            pc_swap16_at(a + 6);
            pc_swap16_at(a + 0xA);
            pc_swap32_at(a + 0xC);
            pc_swap16_n(buf + pos, (size_t)n * 3);
        }
        pos += (size_t)n * 6;
    }

    for (p = 0; p < 4; p++) {
        if (size - pos < 8)
            return 0;
        n = pc_be16(buf + pos);
        if ((size_t)n * 8 > size - pos - 8)
            return 0;
        if (swap) {
            pc_swap16_n(buf + pos, 2);
            pc_swap32_at(buf + pos + 4);
            for (i = 0; i < n; i++)
                pc_swap16_n(buf + pos + 8 + 8 * i, 3);   /* pos[3]; mph and hints are bytes */
        }
        pos += 8 + (size_t)n * 8;
    }
    return 1;
}

void pc_assets_path(int wave, uint8_t *buf, size_t size)
{
    if (walk(buf, size, 0))
        walk(buf, size, 1);
    else
        fprintf(stderr, "rush2049pc: path wave %d: runs past its 0x%zx-byte block, left as is\n", wave, size);
}
