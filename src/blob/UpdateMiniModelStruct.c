/* UpdateMiniModelStruct: a bounding box model follows its target (MAT3 targets.c). */
#include "game/game.h"

/* Sets bounding box model `miniIdx` (gBBoxModelStructs) to orientation `uvs`, position `pos` and velocity `vel`. */
void UpdateMiniModelStruct(s32 miniIdx, f32 *vel, f32 *pos, void *uvs) {
    tMiniModelData *m;
    f32 *v;

    m = &gBBoxModelStructs[miniIdx];
    v = vel;
    CopyUV(uvs, m->uvs);
    m->pos[0] = pos[0];
    m->pos[1] = pos[1];
    m->pos[2] = pos[2];
    m->vel[0] = v[0];
    m->vel[1] = v[1];
    m->vel[2] = v[2];
}
