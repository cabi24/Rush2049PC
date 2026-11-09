/* ZOID_AddObject.c: add a node to the scene (MAT3 zoid.c, ZOID_AddObject()).
 *
 * Nodes live in ObjInstanceList; a free one has index 0xFFFF, and gNumObjects is one past the last node in use. */
#include "game/game.h"


/* Add an instance of object `obj` (a handle, MBOX_FindObject_WORK) with matrix `mat` (ZOID_NewMatrix) and `flags`
 * as the last child of node `parent` (-1: a root): the first free node, else a new one at the end. It starts at
 * scale 1 with the model's own textures, palette and lights and colour 0; the environment colour is left as it
 * was. Returns the node's index. */
s32 ZOID_AddObject(s32 obj, void *mat, s16 parent, s32 flags) {
    ObjectInstance *node;
    s32 i;

    for (i = 0; i < gNumObjects; i++) {
        if (ObjInstanceList[i].index == 0xFFFF) {
            break;
        }
    }
    if (i == gNumObjects) {
        /* None free: the N64 has no limit and writes past the table; the PC says so (pc_pool_overflow: a Debug
         * build stops) and hands out the last one again (docs/port/MORECARS.md 8). */
        if (i >= PC_SCENE_NODES) {
            pc_pool_overflow("the scene nodes", i + 1, PC_SCENE_NODES);
            i = PC_SCENE_NODES - 1;
        } else {
            gNumObjects++;
        }
    }
    if (gMaxObjects < gNumObjects) {
        gMaxObjects = gNumObjects;
    }
    node = &ObjInstanceList[i];
    node->flags = flags;
    node->culling = 0;
    node->mat = mat;
    node->scale = 1.0f;
    node->w = 1.0f;
    node->index = obj;
    node->child = -1;
    node->next = -1;
    node->numLights = -1;
    node->lights = 0;
    node->texture[0] = 0;
    node->texture[1] = 0;
    node->texture[2] = 0;
    node->texture[3] = 0;
    node->tile = 0;
    node->tileSize = 0;
    node->palette = 0;
    node->color = 0;
    ZOID_InsertObject(i, parent);
    return (s16)i;
}
