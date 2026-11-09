/* MBOX_HideObject.c: hide a scene node, or a subtree, in some views or in all (MAT3 mbox.c, MBOX_HideObject()).
 *
 * A node's flags say where it is drawn: OBJ_HIDDEN hides it everywhere, bits 8-11 hide it in views 0-3 (the split
 * screen's windows: OBJ_HIDDEN_VIEW0 << view). MBOX_ShowObject undoes it. */
#include "game/game.h"

/* The ways to hide (`mode`). */
#define HIDE_TREE       2   /* the node and all its descendants, in the views of `views` */
#define HIDE_ALL_VIEWS  0   /* the node alone, everywhere (OBJ_HIDDEN) */
#define HIDE_NODE       1   /* the node alone, in the views of `views` */
#define HIDE_SIBLINGS   3   /* the node, its later siblings and all their descendants, in `views` */

/* Hide node `node` as `mode` says, in the views whose bits `views` has (bit n: view n). */
void MBOX_HideObject(s32 node, s32 mode, u32 views) {
    s32 flags;
    s32 child;

    if (mode == HIDE_TREE) {
        flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = flags | (views << 8);
        if (node < 0) DEBUG_PRINT(("bad object %d\n", node));
        flags = ObjInstanceList[node].child;
        if (flags != -1) MBOX_HideObject(flags, HIDE_SIBLINGS, views);
    } else if (mode == HIDE_ALL_VIEWS) {
        flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = flags | OBJ_HIDDEN;
    } else if (mode == HIDE_NODE) {
        flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = flags | (views << 8);
    } else if (mode == HIDE_SIBLINGS) {
        do {
            flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = flags | (views << 8);
            if (node < 0) DEBUG_PRINT(("bad object %d\n", node));
            child = ObjInstanceList[node].child;
            if (child != -1) MBOX_HideObject(child, HIDE_SIBLINGS, views);
            node = ObjInstanceList[node].next;
        } while (node != -1);
    }
}
