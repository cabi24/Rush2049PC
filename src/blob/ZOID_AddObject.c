/* ZOID_AddObject.c: add a node to the scene (MAT3 zoid.c, ZOID_AddObject()).
 *
 * Nodes live in ObjInstanceList; a free one has index 0xFFFF, and gNumObjects is one past the last node in use. */
#include "game/game.h"

/* The scene node (ObjInstanceList's record, types.h ObjectInstance) with MAT3's field names. */
typedef struct ObjInstance {
    s32 flags;                  /* +0x00 OBJ_* */
    u32 culling;                /* +0x04 its culling box (ZOID_BuildCullingList), 0 none */
    void *mat;                  /* +0x08 its matrix, relative to its parent */
    f32 scale;                  /* +0x10 */
    f32 w;                      /* +0x14 */
    u16 index;                  /* +0x18 the object's handle, (model << 10) | definition; 0xFFFF a free node */
    s16 child, next;            /* +0x1A the first child, the next sibling, -1 none */
    s16 numLights;              /* +0x1E its own lights' count, -1 the scene's */
    void *lights;               /* +0x20 its own lights */
    void *texture[4];           /* +0x28 a texture per level of detail in place of the model's, NULL none */
    void *tile, *tileSize;      /* +0x48 texture tile and tile size in place of the model's */
    void *palette;              /* +0x58 a palette in place of the model's */
    u32 color;                  /* +0x60 the primitive colour */
    s32 envcolor;               /* +0x64 the environment colour */
} ObjInstance;

VIEW(ObjInstance[600], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* Add an instance of object `obj` (a handle, MBOX_FindObject_WORK) with matrix `mat` (ZOID_NewMatrix) and `flags`
 * as the last child of node `parent` (-1: a root): the first free node, else a new one at the end. It starts at
 * scale 1 with the model's own textures, palette and lights and colour 0; the environment colour is left as it
 * was. Returns the node's index. */
s32 ZOID_AddObject(s32 obj, void *mat, s16 parent, s32 flags) {
    ObjInstance *node;
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
