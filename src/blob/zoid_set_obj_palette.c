/* zoid_set_obj_palette.c: give a scene node a palette of its own (the N64's; MAT3 has no palettes). */
#include "game/game.h"

/* The scene node (ObjInstanceList's record, types.h ObjectInstance) with MAT3's field names. */
typedef struct ObjInstance {
    u32 flags, culling;
    void *mat;
    f32 scale, w;
    u16 index;
    s16 child, next, numLights;
    void *lights, *texture[4], *tile, *tileSize;
    uintptr_t palette;          /* +0x58 a palette in place of the model's, 0 none */
    u32 color, envcolor;
} ObjInstance;

VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* Draw node `node` with the palette at `palette` (a palette definition) in place of its model's; 0 gives the
 * model's own back. */
void zoid_set_obj_palette(s16 node, uintptr_t palette) {
    ObjInstanceList[node].palette = palette;
}
