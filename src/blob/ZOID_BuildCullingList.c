/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (also MATCH at -O2) */
/*
 * Append the eight corner vertices of an axis-aligned box (min, max; fixed
 * point x16) to the Vtx pool cull_vertices at the running index cull_vertex_count, tag
 * the 0x44-byte record ObjInstanceList[idx] with (first index << 3) | 7 in its
 * flags word, advance the index by 8 and keep the high-water mark
 * cull_vertex_max.  No arcade ancestor (N64 display-list side).
 *
 * Shaping fact: the vertex pointer is post-incremented after each of the
 * first seven vertices (IDO folds the increments into one `addiu v0,v0,112`
 * with negative offsets); indexing vtx[0..7] leaves 25 aligned rows.
 */
typedef float f32;
typedef short s16;
typedef int s32;
typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct {
    s16 ob[3];
    u16 flag;
    s16 tc[2];
    u8 cn[4];
} Vtx_t;

typedef union {
    Vtx_t v;
    long long force_structure_alignment;
} Vtx;

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct {
    u32 unk0;
    u32 flags;
    void *obj;
    f32 scale, w;
    u16 model;
    s16 child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    u32 prim, env;
} Entry;

extern s32 cull_vertex_count;
extern s32 cull_vertex_max;
extern Vtx cull_vertices[];
extern Entry ObjInstanceList[];

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
