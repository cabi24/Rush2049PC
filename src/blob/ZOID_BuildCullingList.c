#include "game/game.h"

typedef union Vtx {
    Vtx_t v;
    long long force_structure_alignment;
} Vtx;

typedef struct Entry {
    u32 unk0;
    u32 flags;
    void *obj;
    f32 scale, w;
    u16 model;
    s16 child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    u32 prim, env;
} Entry;

VIEW(Vtx[], cull_vertices);
#define cull_vertices cull_vertices_view
VIEW(Entry[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

void ZOID_BuildCullingList(f32 *min, f32 *max, s16 idx) {
    Vtx *vtx;

    vtx = &cull_vertices[cull_vertex_count];
    ObjInstanceList[idx].flags |= (cull_vertex_count << 3) | 7;
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
