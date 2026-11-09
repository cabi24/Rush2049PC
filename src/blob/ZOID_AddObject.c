#include "game/game.h"


#define DEBUG_PRINT(args)

/* PC: the scene node's host layout, which every view of the table has (docs/port/POINTERS.md 9): the nine zeroed
 * words are the light, the four LOD textures, the two texture rectangles, the palette (pointer-sized here) and the
 * primitive colour. */
typedef struct Ent {
    /* 0x00 */ s32 w0;
    /* 0x04 */ s32 w4;
    /* 0x08 */ void *owner;
    /* 0x10 */ f32 scaleA;
    /* 0x14 */ f32 scaleB;
    /* 0x18 */ u16 id;          /* 0xFFFF = free */
    /* 0x1A */ s16 link[3];     /* -1 = none */
    /* 0x20 */ void *light, *tex[4], *rect, *clip, *pal;
    /* 0x60 */ u32 color;
    /* 0x64 */ s32 w64;
} Ent; /* size 0x68 */

VIEW(Ent[600], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the same record by field. The N64 body's byte offset (`off`) and its nine-word clear are N64 layout. */
s32 ZOID_AddObject(s32 a, void *b, s16 parent, s32 d) {
    Ent *e;
    s32 i;

    for (i = 0; i < gNumObjects; i++) {
        if (ObjInstanceList[i].id == 0xFFFF) {
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
    e = &ObjInstanceList[i];
    e->w0 = d;
    e->w4 = 0;
    e->owner = b;
    e->scaleA = 1.0f;
    e->scaleB = 1.0f;
    e->id = a;
    e->link[0] = -1;
    e->link[1] = -1;
    e->link[2] = -1;
    e->light = 0;
    e->tex[0] = 0;
    e->tex[1] = 0;
    e->tex[2] = 0;
    e->tex[3] = 0;
    e->rect = 0;
    e->clip = 0;
    e->pal = 0;
    e->color = 0;
    ZOID_InsertObject(i, parent);
    return (s16)i;
}
