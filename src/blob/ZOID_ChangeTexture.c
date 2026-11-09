/* ZOID_ChangeTexture.c: change the texture an object definition draws with (MAT3 zoidtexture.c,
 * ZOID_ChangeTexture()).
 *
 * Unlike ZOID_SetTexture, which changes one scene node, this changes the object's definition in its model file,
 * so every node that draws the object changes. */
#include "game/game.h"

/* One level of detail of an object definition (MAT3 anon_texidx_flags_c, 16 bytes on the N64). */
typedef struct LodDef { u16 texidx; u16 flags; u8 pad4[4]; PART_DL displayList; u8 padC[4]; } LodDef;
/* An object definition (MAT3 tModelDef): the name, the radius, the levels of detail. */
typedef struct tModelDef { u8 pad0[0x16]; s16 lodCount; LodDef lod[4]; } tModelDef;

/* Make object `obj` (a handle, (model << 10) | definition) draw with texture `texidx` (a texture handle,
 * MBOX_FindTexture_WORK) at level of detail `lod`, or at all its levels when `lod` is negative; LOD_TEXTURE_SET
 * tells the draw to use it. Returns 0 if the object has no level `lod`, else 1. */
s32 ZOID_ChangeTexture(u16 obj, s16 lod, u16 texidx)
{
    tModelDef *def;
    s32 i;

    def = &gRomObjDefPtrs[obj >> 10].romObjDefs[obj & 0x3FF];
    if (lod >= 0) {
        if (lod >= def->lodCount) {
            return 0;
        }
        def->lod[lod].texidx = texidx;
        def->lod[lod].flags |= LOD_TEXTURE_SET;
    } else {
        for (i = 0; i < def->lodCount; i++) {
            def->lod[i].texidx = texidx;
            /* Original game: lod, not i: with lod < 0 this marks def->lod[-1] (the bytes before the table of levels,
             * the definition's header) on every pass and leaves the levels unmarked. MAT3 has the same slip; the
             * callers seen pass lod 0, so it may never run. Kept as retail. */
            def->lod[lod].flags |= LOD_TEXTURE_SET;
        }
    }
    return 1;
}
