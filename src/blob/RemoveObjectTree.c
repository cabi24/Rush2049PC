/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * RemoveObjectTree (historical label): remove scene node idx from the
 * 0x44-byte node table ObjInstanceList (u16 id @0x14, s16 child @0x16, s16 sibling
 * @0x18).  Optionally frees the sibling chain and the child chain (recursive
 * (link, 1, 1)), unlinks idx from whoever references it (ZOID_ObjParent = node
 * whose child is idx, else the list head ObjHead, else ZOID_ObjPrevObj = node
 * whose sibling is idx), marks the node free (id 0xFFFF, links -1) and trims
 * the high-water count gNumObjects.  No arcade ancestor (N64 scene graph).
 *
 * Shaping (from best, two levers):
 *   - a constant-false debug block at entry (`if (SCENE_DEBUG) ...`): it adds
 *     one block to idx's live range, so uopt's priority for idx drops from
 *     1.2 (5 blocks, gets s0) to 1.0 (6 blocks) and idx is split to its home
 * slot (`lh 34(sp)` at every use), as retail (uopt trace);
 *   - QUIRK: ObjHead (scene list head) is declared volatile.  That is what
 *     gives retail's `lui v0; addiu v0; lh 0(v0) ... sh 0(v0)` and the else-
 *     block reload of idx.  The locked ZOID_InsertObject matches with a plain
 *     `extern s16 ObjHead` (with volatile it is 1 word off), so this is not
 * a proven declaration; no non-volatile spelling was found (~80
 * variants, : operand order, *&, debug reads of the head).
 *   - the search results reuse the freeChildren parameter (a named local
 *     costs 8 frame bytes).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Node {
    u32 flags;
    u32 w4;
    void *obj;
    float scale, w;
    /* 0x18 */ u16 id;
    /* 0x1A */ s16 child;
    /* 0x1C */ s16 sibling;
    s16 mode;
    void *light, *tex[4], *rect, *clip, *pal;
    u32 prim, env;
} Node; /* 0x68 */

extern Node ObjInstanceList[];
extern s32 gNumObjects;
extern volatile s16 ObjHead;

s16 ZOID_ObjPrevObj(s16 idx);
s16 ZOID_ObjParent(s16 idx);
#define SCENE_DEBUG 0

void RemoveObjectTree(s16 idx, s32 freeChildren, s32 freeSiblings)
{
    Node *n;

    if (SCENE_DEBUG) {
    }
    if (freeSiblings) {
        n = &ObjInstanceList[idx];
        if (n->sibling >= 0) {
            RemoveObjectTree(n->sibling, 1, 1);
            n->sibling = -1;
        }
    }
    n = &ObjInstanceList[idx];
    if (freeChildren) {
        if (n->child >= 0) {
            RemoveObjectTree(n->child, 1, 1);
            n->child = -1;
        }
    }
    freeChildren = ZOID_ObjParent(idx);
    if (freeChildren >= 0) {
        ObjInstanceList[freeChildren].child = n->sibling;
    } else if (idx == ObjHead) {
        ObjHead = n->sibling;
    } else {
        freeChildren = ZOID_ObjPrevObj(idx);
        if (freeChildren >= 0) {
            ObjInstanceList[freeChildren].sibling = n->sibling;
        }
    }
    n->id = 0xFFFF;
    n->child = -1;
    n->sibling = -1;
    if (idx + 1 == gNumObjects) {
        do {
            gNumObjects--;
        } while (ObjInstanceList[gNumObjects - 1].id == 0xFFFF && gNumObjects > 0);
    }
}
