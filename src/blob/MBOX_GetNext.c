/* MBOX_GetNext.c: a scene node's next sibling (MAT3 mbox.c, MBOX_GetNext()). */
#include "game/game.h"

/* The next sibling of node `node`, or -1. */
s16 MBOX_GetNext(s32 node) {
    return (ObjInstanceList + node)->next;
}
