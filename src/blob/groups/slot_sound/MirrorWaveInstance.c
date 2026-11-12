/* MirrorWaveInstance (MAT3 waves.c): turns a model wave's triangles to face the other way, for a mirrored track
 * (waves/group.c has the waves). */
#include "game/game.h"

/* If wave instance index holds a model whose triangles don't face the way the track now needs (gMirrorWaves), flips
 * every level of detail's display list (flip_triangle_winding through display_list_traverse) and notes it.
 * PC: every level is looked at, as on the N64; a level past the model's lodCount has no display list in the WAD's
 * records (port/assets/wad.c), where the N64 had the file's word unrelocated (which rush2049pc_assetcheck expects to
 * be 0, none). */
void MirrorWaveInstance(s32 index)
{
    tModelDef *m;
    s32 i;
    s32 j;
    s32 count;

    ValidateWaveInstance(index, 1, 1);
    if (gWaveInstance[index].model) {
        if (gWaveInstance[index].mirrored != (gMirrorWaves != 0)) {
            m = gRomObjDefPtrs[index].romObjDefs;
            count = gRomObjDefPtrs[index].count;
            for (i = 0; i < count; i++) {
                for (j = 0; j < 4; j++) {
                    if (m->lod[j].displayList) {
                        display_list_traverse(m->lod[j].displayList, NULL, NULL, 2, flip_triangle_winding);
                    }
                }
                m++;
            }
            gWaveInstance[index].mirrored = (gMirrorWaves != 0);
        }
    }
}
