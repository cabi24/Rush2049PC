/* MirrorWaveInstance (MAT3 waves.c): turns a model wave's triangles to face the other way, for a mirrored track
 * (waves/group.c has the waves). */
#include "game/game.h"
#define ModelList ModelListView   /* this file's own ModelList: include/game's ModelList is another type */

/* PC: the OBHD records (MAT3 tModelDef, 0x58 bytes) keep their N64 layout (port/assets/wad.c): a level of detail's
 * display list is a 32-bit window address. */
#define PART_DL u32
typedef struct { u8 pad0[8]; PART_DL dl; u8 padC[4]; } Part;           /* MAT3 tLod */
typedef struct Model { u8 pad0[0x18]; Part part[4]; } Model;            /* MAT3 tModelDef: name, radius, LODs */
typedef struct { Model *models; s32 count; } ModelList;                 /* a gRomObjDefPtrs entry */

VIEW(ModelList[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view

/* If wave instance index holds a model whose triangles don't face the way the track now needs (gMirrorWaves), flips
 * every level of detail's display list (flip_triangle_winding through display_list_traverse) and notes it. */
void MirrorWaveInstance(s32 index)
{
    Model *m;
    s32 i;
    s32 j;
    s32 count;

    ValidateWaveInstance(index, 1, 1);
    if (gWaveInstance[index].model) {
        if (gWaveInstance[index].mirrored != (gMirrorWaves != 0)) {
            m = gRomObjDefPtrs[index].models;
            count = gRomObjDefPtrs[index].count;
            for (i = 0; i < count; i++) {
                for (j = 0; j < 4; j++) {
                    if (m->part[j].dl) {
                        display_list_traverse(m->part[j].dl, 0, 0, 2, flip_triangle_winding);
                    }
                }
                m++;
            }
            gWaveInstance[index].mirrored = (gMirrorWaves != 0);
        }
    }
}
