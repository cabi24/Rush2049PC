/* ResetCollisionObject: puts a collision object back where the track had it (MAT3 stree.c). */
#include "game/game.h"

/* A surface (MAT3 surftyp, stree_surf[]). */
typedef struct surftyp {
    u16 flags;      /* 0x00 */
    u16 snd_flags;  /* 0x02 */
    s16 uvs[3][3];  /* 0x04 its orientation, 1.0 = 0x4000 */
    u16 vli;        /* 0x16 */
} surftyp;

VIEW(surftyp *, stree_surf);
#define stree_surf stree_surf_view
VIEW(sverttyp *, stree_svert);
#define stree_svert stree_svert_view
VIEW(savetyp *, stree_save);
#define stree_save stree_save_view

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
