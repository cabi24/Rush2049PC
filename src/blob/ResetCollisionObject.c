/* ResetCollisionObject: puts a collision object back where the track had it (MAT3 stree.c). */
#include "game/game.h"

void *memcpy(void *, const void *, u32);

/* Restores each saved surface of collision object `num`: its orientation and its vertex as the track had them
 * (savetyp), undoing MoveCollisionObject and RotateCollisionObject: a target path back at its start, a restart. */
void ResetCollisionObject(s32 num)
{
    savetyp *s;
    s32 i;
    s = stree_save;
    for (i = 0; i < (s32) stree_savenum; i++, s++) {
        if (num == s->object) {
            memcpy(stree_surf[s->snum].uvs, s->uvs, sizeof(s->uvs));
            memcpy(&stree_svert[s->vnum], s->pos, sizeof(sverttyp));
        }
    }
}
