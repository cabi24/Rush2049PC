/* mirror_vertex_texcoords: prepares a model's vertices for a mirrored track, once (relocate_model_dl calls it for a
 * display list's first vertex load). N64 only. */
#include "game/game.h"

/* The game's use of a vertex's flag word (Vtx_t.flag, which the RSP ignores). */
#define VTX_FLAG_MORE 0x8000        /* another vertex of the run follows */
#define VTX_FLAG_DONE 0x4000        /* already prepared */
#define VTX_FLAG_S_SPAN 0x1FF       /* the texture's width the S coordinate mirrors over, in texels */

/* Walks the run of vertices from `vertices` (to the first without VTX_FLAG_MORE) and marks each one done; when
 * mirrored is set, a vertex not yet done gets its S texture coordinate mirrored over its span (span * 32 - s, in the
 * 10.5 fixed point of Vtx_t.tc, kept to 16 bits).
 * PC: the vertices are the ones the WAD's converter copied beside the file, in the host's byte order
 * (port/assets/wad.c), with room after the run. */
void mirror_vertex_texcoords(Vtx *vertices, s32 mirrored) {
    Vtx_t *v;
    u16 flags;
    s32 span;

    v = &vertices->v;
    do {
        flags = v->flag;
        if (!(flags & VTX_FLAG_DONE)) {
            v->flag = (u16)(flags | VTX_FLAG_DONE);
            span = flags & VTX_FLAG_S_SPAN;
            if (mirrored != 0 && span != 0) {
                v->tc[0] = (s16)(u16)(((span << 5) - v->tc[0]) & 0xFFFF);
            }
        }
        v++;
    } while (flags & VTX_FLAG_MORE);
}
