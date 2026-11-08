/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* N64 scene-parent search. Record layout is corroborated by the accepted
 * child/sibling accessors; no complete arcade ancestor is established.
 * Search each record's first-child/sibling chain and return the first owning
 * record. The comparison precedes the -1 terminator check, so querying -1
 * can return an owner as well. Input and return are signed halfwords; the
 * record counter itself is full-width. Requires valid terminating chains. */
typedef signed short s16;
typedef signed int s32;
typedef unsigned char u8;
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
extern SceneRecord ObjInstanceList[];
extern s32 gNumObjects;

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
