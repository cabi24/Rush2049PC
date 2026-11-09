#include "game/game.h"

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct SceneRecord {
    unsigned int flags, w4;
    void *obj;
    float scale, w;
    unsigned short model;
    s16 child;
    s16 sibling;
    s16 mode;
    void *light, *tex[4], *rect, *clip, *pal;
    unsigned int prim, env;
} SceneRecord;
VIEW(SceneRecord[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

s16 ZOID_TrueObjParent(s16 child)
{
    s32 i;
    s16 search_key;
    s32 next;
    s16 first_child;
    s16 sibling;
    SceneRecord *record;

    i = 0;
    if (gNumObjects > 0) {
        search_key = child;
        do {
            record = &ObjInstanceList[i];
            first_child = record->child;
            if (search_key == first_child)
                return i;
            next = first_child;
            if (first_child != -1) {
                do {
                    sibling = ObjInstanceList[next].sibling;
                    if (search_key == sibling)
                        return i;
                    next = sibling;
                } while (sibling != -1);
            }
            i++;
        } while (i < gNumObjects);
    }
    return -1;
}
