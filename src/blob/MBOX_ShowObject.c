/* MBOX_ShowObject.c: show a scene node, or a subtree, again (MAT3 mbox.c, MBOX_ShowObject()).
 *
 * The counterpart of MBOX_HideObject: clears OBJ_HIDDEN and the views' bits 8-11. */
#include "game/game.h"

#define DEBUG_PRINT(args)

/* The ways to show (`mode`): note the numbers differ from MBOX_HideObject's. */
#define SHOW_NODE       0   /* the node alone */
#define SHOW_TREE       1   /* the node and all its descendants */
#define SHOW_SIBLINGS   2   /* the node, its later siblings and all their descendants */

/* Show node `node` as `mode` says: in the views whose bits `views` has (bit n: view n), and no longer hidden
 * everywhere. */
void MBOX_ShowObject(s32 node, s32 mode, u32 views) {
    s32 flags;
    s32 child;

    if (mode == SHOW_TREE) {
        flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = flags & ~((views << 8) | OBJ_HIDDEN);
        if (node < 0) DEBUG_PRINT(("bad object %d\n", node));
        flags = ObjInstanceList[node].child;
        if (flags != -1) MBOX_ShowObject(flags, SHOW_SIBLINGS, views);
    } else if (mode == SHOW_NODE) {
        flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = flags & ~((views << 8) | OBJ_HIDDEN);
    } else if (mode == SHOW_NODE) {
        /* Original game: never reached, mode 0 is tested twice (MAT3 too); this would show the node everywhere but
         * hide it in `views`. */
        flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = (flags & ~OBJ_HIDDEN) | (views << 8);
    } else if (mode == SHOW_SIBLINGS) {
        do {
            flags = ObjInstanceList[(s16) node].flags; ObjInstanceList[node].flags = flags & ~((views << 8) | OBJ_HIDDEN);
            if (node < 0) DEBUG_PRINT(("bad object %d\n", node));
            child = ObjInstanceList[node].child;
            if (child != -1) MBOX_ShowObject(child, SHOW_SIBLINGS, views);
            node = ObjInstanceList[node].next;
        } while (node != -1);
    }
}
