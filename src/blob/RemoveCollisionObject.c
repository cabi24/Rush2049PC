/* RemoveCollisionObject: takes a collision object out of the road (MAT3 stree.c). */
#include "game/game.h"

/* A surface (MAT3 surftyp, stree_surf[]): its flags. */
typedef struct surftyp { u16 flags; u8 pad2[22]; } surftyp;

VIEW(savetyp *, stree_save);
#define stree_save stree_save_view
VIEW(surftyp *, stree_surf);
#define stree_surf stree_surf_view

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
