/* ZOID_BuildCullingList.c: give a scene node a bounding box for view culling (the N64's own: MAT3's
 * ZOID_BuildCullingList in zoid.c is empty).
 *
 * The box's eight corners go into cull_vertices, the vertex list the scene draw loads before a culled node's
 * display list; the node's culling word says where they are. */
#include "game/game.h"


/* Give node `node` the box from min[3] to max[3] (feet, in its own frame): its eight corners in the libultra
 * vertex format the RSP culls by (16 units a foot), min/max in z alternating fastest, then y, then x. */
void ZOID_BuildCullingList(f32 *min, f32 *max, s16 node) {
    Vtx *vtx;

    vtx = &cull_vertices[cull_vertex_count];
    ObjInstanceList[node].culling |= (cull_vertex_count << 3) | 7;
    cull_vertex_count += 8;
    if (cull_vertex_max < cull_vertex_count) {
        cull_vertex_max = cull_vertex_count;
    }
    vtx->v.ob[0] = min[0] * 16.0f;
    vtx->v.ob[1] = min[1] * 16.0f;
    vtx->v.ob[2] = min[2] * 16.0f;
    vtx++;
    vtx->v.ob[0] = min[0] * 16.0f;
    vtx->v.ob[1] = min[1] * 16.0f;
    vtx->v.ob[2] = max[2] * 16.0f;
    vtx++;
    vtx->v.ob[0] = min[0] * 16.0f;
    vtx->v.ob[1] = max[1] * 16.0f;
    vtx->v.ob[2] = min[2] * 16.0f;
    vtx++;
    vtx->v.ob[0] = min[0] * 16.0f;
    vtx->v.ob[1] = max[1] * 16.0f;
    vtx->v.ob[2] = max[2] * 16.0f;
    vtx++;
    vtx->v.ob[0] = max[0] * 16.0f;
    vtx->v.ob[1] = min[1] * 16.0f;
    vtx->v.ob[2] = min[2] * 16.0f;
    vtx++;
    vtx->v.ob[0] = max[0] * 16.0f;
    vtx->v.ob[1] = min[1] * 16.0f;
    vtx->v.ob[2] = max[2] * 16.0f;
    vtx++;
    vtx->v.ob[0] = max[0] * 16.0f;
    vtx->v.ob[1] = max[1] * 16.0f;
    vtx->v.ob[2] = min[2] * 16.0f;
    vtx++;
    vtx->v.ob[0] = max[0] * 16.0f;
    vtx->v.ob[1] = max[1] * 16.0f;
    vtx->v.ob[2] = max[2] * 16.0f;
}
