/* zoid_set_obj_tile_size.c: give a scene node a texture tile size of its own (the N64's; MAT3 keeps the field,
 * tileSize, but not the setter). */
#include "game/game.h"

/* The scene node (ObjInstanceList's record, types.h ObjectInstance) with MAT3's field names. */
typedef struct ObjInstance {
    u32 flags, culling;
    void *mat;
    f32 scale, w;
    u16 index;
    s16 child, next, numLights;
    void *lights, *texture[4], *tile;
    uintptr_t tileSize;         /* +0x50 a texture tile size in place of the model's, 0 none */
    void *palette;
    u32 color, envcolor;
} ObjInstance;
VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* Draw node `node` with the texture tile size at `tileSize` in place of its model's (0: the model's). The third
 * argument is not used. */
void zoid_set_obj_tile_size(short node, uintptr_t tileSize, u32 unused)
{
    ObjInstanceList[node].tileSize = tileSize;
}
