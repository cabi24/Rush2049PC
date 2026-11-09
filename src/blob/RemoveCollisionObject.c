/* RemoveCollisionObject: takes a collision object out of the road (MAT3 stree.c). */
#include "game/game.h"

/* Gives every surface of collision object `num` the flags 15, which the collision tests pass by: a target path
 * whose target isn't in this race (SetTargetDefs). */
void RemoveCollisionObject(u32 num) {
    s32 i;
    savetyp *s = stree_save;
    for (i = 0; i < stree_savenum; i++, s++) {
        if (s->object == num) {
            stree_surf[s->snum].flags = 15;
        }
    }
}
