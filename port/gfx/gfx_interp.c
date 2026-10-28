/* port/gfx/gfx_interp.c: frame interpolation, for showing more frames than the game draws (video.frame_rate;
 * docs/port/GFX.md "High frame rate").
 *
 * The game draws at most one frame every two retraces (30 fps; its scheduler swaps no sooner) and steps its world
 * by whole retraces, so it is left alone: this file draws extra frames between the game's own. While it is on, each
 * graphics task is recorded as the interpreter runs it (the live drawing is unchanged):
 *   - every vertex load: the vertices in model space, as loaded, and the transform state they were loaded under
 *     (modelview, projection, their product); a new transform whenever one of them changes;
 *   - every triangle (also those the frame culls or rejects, when their draw state is at hand: another moment may
 *     show them), with what turning it into GL vertices needs (the viewport, culling, depth source, draw state);
 *   - rectangles as their GL vertices, and clears.
 * Between the frame on screen (B) and the next one, already drawn (C), a frame at fraction a of the way is drawn
 * again from the recording of the nearer one (B up to halfway, then C), into a target of its own, with each 3D
 * transform moved toward the other frame's: paired by what it drew (the view, and the vertex sources loaded under
 * it, which are the same model data from one frame to the next; instances of a model by their order under a paired
 * parent, or by place), its modelview and the view's projection are blended to the moment. 2D (rectangles,
 * orthographic triangles) is drawn as that frame has it. Transforms the other frame lacks, or whose change is too
 * large for motion (a teleport), stay where that frame has them in the world, seen through the camera at the
 * moment (its move measured from the world's pieces); a view whose camera jumped (a cut) holds B until C is due.
 * Nothing here touches the game's memory: the recording is all it reads. The mechanics and their tests: GFX.md
 * "High frame rate"; the reasons: docs/port/FRAMERATE.md. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

void *pcgfx_rec;

#define HFR_FRAMES 4
#define NONE 0xFFFFFFFFu

enum { V_FOG = 1, V_FIXED = 2, V_DYN = 4 };              /* RVtx.flags */
enum { XF_SNAP = 0, XF_LERP = 1, XF_CAMERA = 2 };        /* Xf.mode */
enum { OP_TRIS, OP_VERTS, OP_CLEAR };

/* A transform state some vertices were loaded under. */
typedef struct {
    float mv[4][4], proj[4][4], mp[4][4];
    float bmin[3], bmax[3];     /* its vertices' model-space bounds */
    uint64_t sig;               /* the view, and each load's source and size */
    float xs;                   /* the outcodes' widening when they were loaded (view_x) */
    uint32_t gen, nload;
    int32_t parent;             /* the transform whose modelview is the one this was pushed from, -1 */
    uint16_t depth;             /* the modelview stack's depth */
    uint16_t view, vp;
    int16_t fog_mul, fog_off;
    uint8_t persp, decomposed, has_dyn;
    /* as the frame drawn again */
    int32_t match;              /* its partner's transform in the other frame, -1 */
    uint8_t mode, eye_fixed, spin;   /* spin: turned too far to blend; its place is blended, its turn is its own */
    float now[4][4], eye[4][4]; /* the drawn moment's MP; and for eye-relative native vertices */
} Xf;

typedef struct {
    float o[3];                 /* model space */
    float clip[4];              /* as loaded */
    float rgba[4];              /* shade (alpha is fog when V_FOG) */
    float s, t;
    uint32_t xf;
    uint8_t flags;
} RVtx;

typedef struct {
    uint32_t v[3], state, ctx;
} RTri;

typedef struct {
    GfxTriCtx c;
    uint16_t vp;                /* viewport ordinal (0: the default one) */
    int16_t frame_w, ox;        /* widescreen: the frame's width and its widening (view_x again) */
    uint8_t persp;
} RCtx;

typedef struct {
    GfxDrawState st;
    GfxUnitInfo units[2];
} RState;

typedef struct {
    uint8_t kind, depth;
    uint32_t state, first, count;
    int32_t rect[4];
    float rgba[4], z;
} ROp;

typedef struct {
    float scale[3], trans[3];
} RVp;

typedef struct {
    float proj[4][4];
    uint8_t persp, seen;
} RView;

#define ARRAY(type, name) \
    type *name;           \
    uint32_t n_##name, c_##name

typedef struct {
    int used, ok;
    uint32_t seq;
    int64_t shown;              /* the retrace it was swapped in at; -1 not yet */
    GfxTarget *target;          /* the color image it drew */
    int tw, th;
    float scale;                /* the internal scale the target was drawn at */
    uint32_t n3d;               /* perspective transforms that can move */
    ARRAY(Xf, xf);
    ARRAY(RVtx, vtx);
    ARRAY(RTri, tri);
    ARRAY(RCtx, ctx);
    ARRAY(RState, st);
    ARRAY(ROp, op);
    ARRAY(GfxOutVtx, out);
    ARRAY(RVp, vp);
    ARRAY(RView, view);
    uint32_t slot[32];          /* the vertex buffer: vtx index of each slot */
    uint32_t cur_xf;
} RFrame;

/* per view of the frame drawn again, for a pair */
typedef struct {
    float de[3];                /* the modelviews' change from it to its partner by the camera's move */
    float proj[4][4];           /* the projection at the drawn moment */
    uint8_t cut, matched;
} PView;

static struct {
    RFrame f[HFR_FRAMES];
    uint32_t seq;
    RFrame *cur;                /* the frame on screen */
    int64_t prev_shown;         /* when the one before it was */
    GfxTarget *aux;
    PView *pv;
    uint32_t n_pv;
    uint32_t moving;            /* the pair's transforms that move (LERP, CAMERA) */
    uint32_t match_r, match_o;  /* the frame drawn again and its partner the matching above is for (seq) */
    GfxOutVtx *batch;
    uint32_t n_batch, c_batch;
    int batch_state;
    GfxVtx *gv;                 /* the drawn frame's vertices at the moment */
    uint32_t c_gv;
    PcGfxHfrStats stats;
    const char *dump;           /* RUSH_GFX_HFR_DUMP */
} H;

/* ---------------------------------------------------------------- small helpers */

static int grow(void **p, uint32_t *cap, uint32_t need, size_t size) {
    uint32_t c;
    void *q;
    if (need <= *cap)
        return 0;
    c = *cap ? *cap : 64;
    while (c < need)
        c *= 2;
    q = realloc(*p, (size_t)c * size);
    if (q == NULL)
        return -1;
    *p = q;
    *cap = c;
    return 0;
}

#define PUSH(f, name) \
    (grow((void **)&(f)->name, &(f)->c_##name, (f)->n_##name + 1, sizeof(*(f)->name)) ? NULL \
                                                                                        : &(f)->name[(f)->n_##name++])

static uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h * 0xFF51AFD7ED558CCDull;
}

/* the renderer's MV x P (gfx_rsp.c's mtx_mul, operation for operation, so that a transform that didn't move gives
 * the same MP to the bit) */
static void mtx_mul(float out[4][4], const float a[4][4], const float b[4][4]) {
    float r[4][4];
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
    memcpy(out, r, sizeof(r));
}

static void mtx_lerp(float out[4][4], const float a[4][4], const float b[4][4], float t) {
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            out[i][j] = a[i][j] + (b[i][j] - a[i][j]) * t;
}

static float len3(const float *v) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* ---------------------------------------------------------------- recording */

static void frame_clear(RFrame *f) {
    f->ok = 1;
    f->shown = -1;
    f->target = NULL;
    f->n3d = 0;
    f->n_xf = f->n_vtx = f->n_tri = f->n_ctx = f->n_st = f->n_op = f->n_out = f->n_vp = f->n_view = 0;
    memset(f->slot, 0xFF, sizeof(f->slot));
    f->cur_xf = NONE;
}

static void frame_free(RFrame *f) {
    free(f->xf);
    free(f->vtx);
    free(f->tri);
    free(f->ctx);
    free(f->st);
    free(f->op);
    free(f->out);
    free(f->vp);
    free(f->view);
    memset(f, 0, sizeof(*f));
}

void pcgfx_rec_task_begin(void) {
    RFrame *f = NULL;
    int i;
    /* a slot that is neither on screen nor waiting to be: an unused one, else the oldest of the rest */
    for (i = 0; i < HFR_FRAMES; i++) {
        RFrame *g = &H.f[i];
        if (g == H.cur)
            continue;
        if (g->used && H.cur != NULL && g->seq > H.cur->seq && g->shown < 0)
            continue;   /* drawn, not shown yet: it may be the next */
        if (f == NULL || (f->used && (!g->used || g->seq < f->seq)))
            f = g;
    }
    if (f == NULL) {   /* every other slot is waiting: the oldest of them */
        for (i = 0; i < HFR_FRAMES; i++)
            if (&H.f[i] != H.cur && (f == NULL || H.f[i].seq < f->seq))
                f = &H.f[i];
    }
    frame_clear(f);
    f->used = 1;
    f->seq = ++H.seq;
    pcgfx_rec = f;
}

static void note_target(RFrame *f, const GfxTarget *t) {
    if (t == NULL)
        return;
    if (f->target == NULL)
        f->target = (GfxTarget *)t;
    else if (f->target != t) {
        static int told;
        if (f->ok && pcgfx_cfg.verbose && !told++)
            fprintf(stderr, "gfx: a frame draws into two color images: frames like it are shown as they are\n");
        f->ok = 0;   /* a frame that draws two color images: shown as it is */
    }
}

void pcgfx_rec_task_end(void) {
    RFrame *f = (RFrame *)pcgfx_rec;
    uint32_t i;
    pcgfx_rec = NULL;
    if (f == NULL)
        return;
    if (f->target == NULL || f->n_op == 0) {
        f->ok = 0;
        return;
    }
    pcgfx_gl_target_dims(f->target, &f->tw, &f->th);
    f->scale = pcgfx_gl_target_scale(f->target);
    for (i = 0; i < f->n_xf; i++)
        if (f->xf[i].persp && f->xf[i].decomposed)
            f->n3d++;
    if (pcgfx_cfg.verbose > 2)
        fprintf(stderr, "gfx: frame %u recorded: %u transforms (%u 3D), %u vertices, %u triangles, %u ops%s\n", f->seq,
                f->n_xf, f->n3d, f->n_vtx, f->n_tri, f->n_op, f->ok ? "" : ", not to be drawn again");
}

static uint32_t current_xf(RFrame *f) {
    Xf *x;
    float xs = 1.0f;
    if (f->cur_xf != NONE) {
        x = &f->xf[f->cur_xf];
        if (x->gen == pcgfx.xf_gen && x->vp == pcgfx.vp_ord && x->fog_mul == pcgfx.fog_mul &&
            x->fog_off == pcgfx.fog_off)
            return f->cur_xf;
    }
    x = PUSH(f, xf);
    if (x == NULL) {
        f->ok = 0;
        return NONE;
    }
    memset(x, 0, sizeof(*x));
    memcpy(x->mv, pcgfx.mv[pcgfx.mv_top], sizeof(x->mv));
    memcpy(x->proj, pcgfx.proj, sizeof(x->proj));
    memcpy(x->mp, pcgfx.mp, sizeof(x->mp));   /* load_vertices brought it up to date */
    x->persp = (uint8_t)pcgfx.persp;
    x->decomposed = !pcgfx.mp_forced;
    x->gen = pcgfx.xf_gen;
    x->view = pcgfx.view_ord;
    x->vp = pcgfx.vp_ord;
    x->fog_mul = pcgfx.fog_mul;
    x->fog_off = pcgfx.fog_off;
    x->bmin[0] = x->bmin[1] = x->bmin[2] = 1e30f;
    x->bmax[0] = x->bmax[1] = x->bmax[2] = -1e30f;
    x->sig = mix(0x5EED, x->view);
    x->match = -1;
    /* its parent: the latest transform one level up with the modelview this one was pushed from (a car's body for
     * its wheels) */
    x->depth = (uint16_t)pcgfx.mv_top;
    x->parent = -1;
    if (pcgfx.mv_top > 0 && !pcgfx.mp_forced) {
        uint32_t k, lim = f->n_xf - 1 > 256 ? f->n_xf - 1 - 256 : 0;
        for (k = f->n_xf - 1; k-- > lim;) {
            const Xf *p = &f->xf[k];
            if (p->depth + 1 == x->depth && p->view == x->view &&
                memcmp(p->mv, pcgfx.mv[pcgfx.mv_top - 1], sizeof(p->mv)) == 0) {
                x->parent = (int32_t)k;
                break;
            }
            if (p->depth < x->depth - 1)
                break;   /* past the parent's level: it drew nothing */
        }
    }
    /* the outcodes' widening, as load_vertices had it: the vertex buffer's outcodes are recomputed with it */
    if (pcgfx.wide_ox != 0 && pcgfx.persp) {
        float h = fabsf(pcgfx.vp_scale[0]), l, r;
        l = pcgfx_wide_x(pcgfx.vp_trans[0] - h);
        r = pcgfx_wide_x(pcgfx.vp_trans[0] + h);
        if (h > 0.0f)
            xs = 2.0f * h / (r - l);
    }
    x->xs = xs;
    while (f->n_view <= x->view) {
        RView *v = PUSH(f, view);
        if (v == NULL) {
            f->ok = 0;
            break;
        }
        memset(v, 0, sizeof(*v));
    }
    if (x->view < f->n_view) {
        RView *v = &f->view[x->view];
        memcpy(v->proj, pcgfx.proj, sizeof(v->proj));
        v->persp = x->persp;
        v->seen = 1;
    }
    f->cur_xf = f->n_xf - 1;
    return f->cur_xf;
}

void pcgfx_rec_load(const uint8_t *p, int be, int v0, int n) {
    RFrame *f = (RFrame *)pcgfx_rec;
    uint32_t xi;
    Xf *x;
    int i, k;
    xi = current_xf(f);
    if (xi == NONE)
        return;
    x = &f->xf[xi];
    x->sig = mix(mix(x->sig, (uint64_t)(uintptr_t)p), (uint64_t)n << 1 | (be != 0));
    x->nload++;
    if (!be)
        x->has_dyn = 1;
    for (i = 0; i < n; i++) {
        const GfxVtx *g = &pcgfx.vtx[v0 + i];
        RVtx *r = PUSH(f, vtx);
        if (r == NULL) {
            f->ok = 0;
            return;
        }
        x = &f->xf[xi];   /* PUSH may not move it, but keep it simple */
        r->o[0] = (int16_t)pcgfx_rd16(p + 16 * i + 0, be);
        r->o[1] = (int16_t)pcgfx_rd16(p + 16 * i + 2, be);
        r->o[2] = (int16_t)pcgfx_rd16(p + 16 * i + 4, be);
        r->clip[0] = g->x;
        r->clip[1] = g->y;
        r->clip[2] = g->z;
        r->clip[3] = g->w;
        r->rgba[0] = g->r;
        r->rgba[1] = g->g;
        r->rgba[2] = g->b;
        r->rgba[3] = g->a;
        r->s = g->s;
        r->t = g->t;
        r->xf = xi;
        r->flags = (uint8_t)(((pcgfx.geom & G_FOG) ? V_FOG : 0) | (be ? 0 : V_DYN));
        for (k = 0; k < 3; k++) {
            if (r->o[k] < x->bmin[k]) x->bmin[k] = r->o[k];
            if (r->o[k] > x->bmax[k]) x->bmax[k] = r->o[k];
        }
        f->slot[v0 + i] = f->n_vtx - 1;
    }
}

void pcgfx_rec_modify(unsigned idx, int moved) {
    RFrame *f = (RFrame *)pcgfx_rec;
    const GfxVtx *g = &pcgfx.vtx[idx];
    RVtx *r;
    uint32_t old = f->slot[idx];
    if (old == NONE)
        return;
    r = PUSH(f, vtx);   /* a copy: the triangles already recorded keep the vertex as it was */
    if (r == NULL) {
        f->ok = 0;
        return;
    }
    *r = f->vtx[old];
    r->clip[0] = g->x;
    r->clip[1] = g->y;
    r->clip[2] = g->z;
    r->clip[3] = g->w;
    r->rgba[0] = g->r;
    r->rgba[1] = g->g;
    r->rgba[2] = g->b;
    r->rgba[3] = g->a;
    r->s = g->s;
    r->t = g->t;
    r->flags &= (uint8_t)~V_FOG;   /* its alpha is what was set, not fog any more */
    if (moved)
        r->flags |= V_FIXED;       /* placed on screen: drawn where C has it */
    f->slot[idx] = f->n_vtx - 1;
}

static uint32_t intern_state(RFrame *f, const GfxDrawState *st, const GfxUnitInfo *units) {
    RState *s;
    if (f->n_st > 0) {
        s = &f->st[f->n_st - 1];
        if (memcmp(&s->st, st, sizeof(*st)) == 0 && memcmp(s->units, units, sizeof(s->units)) == 0)
            return f->n_st - 1;
    }
    s = PUSH(f, st);
    if (s == NULL) {
        f->ok = 0;
        return NONE;
    }
    s->st = *st;
    memcpy(s->units, units, sizeof(s->units));
    return f->n_st - 1;
}

static ROp *add_op(RFrame *f, int kind, uint32_t state, uint32_t first) {
    ROp *o;
    if (f->n_op > 0) {
        o = &f->op[f->n_op - 1];
        if (o->kind == kind && kind != OP_CLEAR && o->state == state && o->first + o->count == first)
            return o;
    }
    o = PUSH(f, op);
    if (o == NULL) {
        f->ok = 0;
        return NULL;
    }
    memset(o, 0, sizeof(*o));
    o->kind = (uint8_t)kind;
    o->state = state;
    o->first = first;
    return o;
}

void pcgfx_rec_tri(unsigned i0, unsigned i1, unsigned i2, const GfxTriCtx *c, const GfxDrawState *st) {
    RFrame *f = (RFrame *)pcgfx_rec;
    uint32_t si, ci;
    RTri *t;
    RCtx *rc;
    ROp *o;
    if (st == NULL && (st = pcgfx_draw_state_peek(-1, c->wide)) == NULL)
        return;   /* rejected here, and its draw state isn't at hand: left out */
    if (f->slot[i0] == NONE || f->slot[i1] == NONE || f->slot[i2] == NONE)
        return;
    note_target(f, st->target);
    si = intern_state(f, st, pcgfx_units());
    if (si == NONE)
        return;
    rc = f->n_ctx > 0 ? &f->ctx[f->n_ctx - 1] : NULL;
    if (rc == NULL || memcmp(&rc->c, c, sizeof(*c)) != 0 || rc->vp != pcgfx.vp_ord) {
        rc = PUSH(f, ctx);
        if (rc == NULL) {
            f->ok = 0;
            return;
        }
        memset(rc, 0, sizeof(*rc));
        rc->c = *c;
        rc->vp = pcgfx.vp_ord;
        rc->frame_w = (int16_t)pcgfx_frame_width();
        rc->ox = (int16_t)pcgfx.wide_ox;
        rc->persp = (uint8_t)pcgfx.persp;
    }
    ci = f->n_ctx - 1;
    t = PUSH(f, tri);
    if (t == NULL) {
        f->ok = 0;
        return;
    }
    t->v[0] = f->slot[i0];
    t->v[1] = f->slot[i1];
    t->v[2] = f->slot[i2];
    t->state = si;
    t->ctx = ci;
    o = add_op(f, OP_TRIS, si, f->n_tri - 1);
    if (o != NULL)
        o->count++;
}

void pcgfx_rec_verts(const GfxDrawState *st, const GfxOutVtx *v, int n) {
    RFrame *f = (RFrame *)pcgfx_rec;
    uint32_t si, first = f->n_out;
    ROp *o;
    int i;
    note_target(f, st->target);
    si = intern_state(f, st, pcgfx_units());
    if (si == NONE)
        return;
    for (i = 0; i < n; i++) {
        GfxOutVtx *d = PUSH(f, out);
        if (d == NULL) {
            f->ok = 0;
            return;
        }
        *d = v[i];
    }
    o = add_op(f, OP_VERTS, si, first);
    if (o != NULL)
        o->count += (uint32_t)n;
}

void pcgfx_rec_clear(const GfxTarget *t, const int32_t rect[4], int depth, const float rgba[4], float z) {
    RFrame *f = (RFrame *)pcgfx_rec;
    ROp *o;
    if (!depth)
        note_target(f, t);   /* a depth clear goes through whichever target was last: any depth is the frame's */
    o = add_op(f, OP_CLEAR, 0, 0);
    if (o == NULL)
        return;
    o->depth = (uint8_t)(depth != 0);
    memcpy(o->rect, rect, sizeof(o->rect));
    if (rgba != NULL)
        memcpy(o->rgba, rgba, sizeof(o->rgba));
    o->z = z;
}

void pcgfx_rec_viewport(void) {
    RFrame *f = (RFrame *)pcgfx_rec;
    RVp *v = PUSH(f, vp);
    if (v == NULL) {
        f->ok = 0;
        return;
    }
    memcpy(v->scale, pcgfx.vp_scale, sizeof(v->scale));
    memcpy(v->trans, pcgfx.vp_trans, sizeof(v->trans));
}

/* ---------------------------------------------------------------- matching a pair */

/* Tuning (GFX.md "High frame rate"). Units are the modelview's: the game's are 1/16 foot. Motion limits are per
 * retrace, multiplied by the retraces between the two frames. */
#define CUT_ANGLE_COS 0.70710678f    /* a view's projection turned more than 45 degrees: a camera cut */
#define CUT_FOV_RATIO 1.5f           /* or zoomed more than this */
#define CUT_MOVE_PER_RETRACE 150.0f  /* or its camera moved this far (9 ft a retrace: 380 mph) */
#define CUT_MOVE_REL 0.35f           /* or this share of the median distance to what it shows */
#define CUT_VOTE 0.3f                /* or this share of its matched transforms jumped */
#define JUMP_ANGLE_COS 0.64278761f   /* a transform turned more than 50 degrees: a jump, not motion */
#define JUMP_SCALE 1.67f             /* or grew or shrank by more than this */
#define JUMP_MOVE_PER_RETRACE 100.0f /* or moved against the camera this far (6 ft a retrace), */
#define JUMP_MOVE_REL 0.6f           /* and more than this share of its distance */
#define STILL_COS 0.99999f           /* a rotation this close counts as unchanged (the camera's own samples) */

typedef struct {
    uint64_t sig;
    uint32_t idx;
} SigIdx;

static int cmp_sig(const void *a, const void *b) {
    const SigIdx *x = (const SigIdx *)a, *y = (const SigIdx *)b;
    if (x->sig != y->sig)
        return x->sig < y->sig ? -1 : 1;
    return x->idx < y->idx ? -1 : x->idx > y->idx;
}

static int cmp_float(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

static float median(float *v, uint32_t n) {
    if (n == 0)
        return 0.0f;
    qsort(v, n, sizeof(*v), cmp_float);
    return n & 1 ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

/* The rows of the modelview's 3x3 (where the model's axes go): turned or scaled too much for one frame? */
static int rotation_jumped(const float a[4][4], const float b[4][4], int *still) {
    int k, jumped = 0;
    *still = 1;
    for (k = 0; k < 3; k++) {
        float la = len3(a[k]), lb = len3(b[k]), c;
        if (la < 1e-6f || lb < 1e-6f) {
            if ((la < 1e-6f) != (lb < 1e-6f))
                jumped = 1, *still = 0;
            continue;
        }
        c = (a[k][0] * b[k][0] + a[k][1] * b[k][1] + a[k][2] * b[k][2]) / (la * lb);
        if (c < JUMP_ANGLE_COS || la > lb * JUMP_SCALE || lb > la * JUMP_SCALE)
            jumped = 1;
        if (c < STILL_COS || fabsf(la - lb) > 1e-4f * la)
            *still = 0;
    }
    return jumped;
}

/* The projection (with the camera's turn the game multiplies into it): where each world axis goes, by its x, y and
 * w (z only scales depth). Turned or zoomed too much for one frame: a cut. */
static int projection_jumped(const float a[4][4], const float b[4][4]) {
    int k;
    for (k = 0; k < 3; k++) {
        float u[3] = {a[k][0], a[k][1], a[k][3]}, v[3] = {b[k][0], b[k][1], b[k][3]};
        float lu = len3(u), lv = len3(v), c;
        if (lu < 1e-9f && lv < 1e-9f)
            continue;
        if (lu < 1e-9f || lv < 1e-9f)
            return 1;
        c = (u[0] * v[0] + u[1] * v[1] + u[2] * v[2]) / (lu * lv);
        if (c < CUT_ANGLE_COS || lu > lv * CUT_FOV_RATIO || lv > lu * CUT_FOV_RATIO)
            return 1;
    }
    return 0;
}

static void center(const Xf *x, float c[3]) {
    int k;
    for (k = 0; k < 3; k++)
        c[k] = x->bmin[k] <= x->bmax[k] ? 0.5f * (x->bmin[k] + x->bmax[k]) : 0.0f;
}

/* The bounds' centre after the modelview: where the object is, relative to the camera. */
static void eye_pos(const Xf *x, float e[3]) {
    float c[3];
    int k;
    center(x, c);
    for (k = 0; k < 3; k++)
        e[k] = c[0] * x->mv[0][k] + c[1] * x->mv[1][k] + c[2] * x->mv[2][k] + x->mv[3][k];
}

/* How far apart the two are against the camera (their eye_pos), the older one moved by the camera's move de. */
static float moved_dist2(const float eb[3], const float ec[3], const float de[3]) {
    float d2 = 0.0f;
    int k;
    for (k = 0; k < 3; k++)
        d2 += (eb[k] - de[k] - ec[k]) * (eb[k] - de[k] - ec[k]);
    return d2;
}

static void match_pair(RFrame *b, RFrame *c, float period) {
    SigIdx *sb, *sc;
    float(*eb)[3], (*ec)[3];
    uint32_t i, j, nv = c->n_view;
    float *samples[3];
    uint32_t ns;

    free(H.pv);
    H.pv = (PView *)calloc(nv ? nv : 1, sizeof(PView));
    H.n_pv = nv;
    for (i = 0; i < c->n_xf; i++) {
        c->xf[i].match = -1;
        c->xf[i].mode = XF_SNAP;
        c->xf[i].eye_fixed = 0;
        c->xf[i].spin = 0;
    }
    if (H.pv == NULL)
        return;

    /* 1. Transforms by what they drew. A signature drawn once in each frame is a pair; one drawn several times
     *    (instances of a model: cars, particles) pairs each with its nearest, after the camera's move is known. */
    sb = (SigIdx *)malloc(sizeof(SigIdx) * (b->n_xf + 1));
    sc = (SigIdx *)malloc(sizeof(SigIdx) * (c->n_xf + 1));
    if (sb == NULL || sc == NULL) {
        free(sb);
        free(sc);
        return;
    }
    for (i = 0; i < b->n_xf; i++)
        sb[i].sig = b->xf[i].sig, sb[i].idx = i;
    for (i = 0; i < c->n_xf; i++)
        sc[i].sig = c->xf[i].sig, sc[i].idx = i;
    qsort(sb, b->n_xf, sizeof(*sb), cmp_sig);
    qsort(sc, c->n_xf, sizeof(*sc), cmp_sig);

    /* 2. The camera's move per view, from transforms drawn once in each frame that didn't turn (the world's
     *    pieces): the median of their move. */
    for (j = 0; j < 3; j++)
        samples[j] = (float *)malloc(sizeof(float) * (c->n_xf + 1));
    for (i = 0, j = 0; i < c->n_xf && samples[0] && samples[1] && samples[2];) {
        uint32_t i2 = i, j2;
        while (i2 < c->n_xf && sc[i2].sig == sc[i].sig)
            i2++;
        while (j < b->n_xf && sb[j].sig < sc[i].sig)
            j++;
        j2 = j;
        while (j2 < b->n_xf && sb[j2].sig == sc[i].sig)
            j2++;
        if (i2 - i == 1 && j2 - j == 1) {
            Xf *xc = &c->xf[sc[i].idx], *xb = &b->xf[sb[j].idx];
            if (xc->view == xb->view)
                xc->match = (int32_t)sb[j].idx;
        }
        i = i2;
        j = j2;
    }
    for (j = 0; j < nv; j++) {
        uint32_t k;
        PView *pv = &H.pv[j];
        float *d = samples[0] ? samples[0] : NULL;
        ns = 0;
        if (d != NULL)
            for (i = 0; i < c->n_xf; i++) {
                Xf *xc = &c->xf[i], *xb;
                int still;
                /* native vertex data counts too: the game's world pieces load their culling boxes from it */
                if (xc->view != j || xc->match < 0 || !xc->persp || !xc->decomposed)
                    continue;
                xb = &b->xf[xc->match];
                if (!xb->persp || !xb->decomposed)
                    continue;
                rotation_jumped(xb->mv, xc->mv, &still);
                if (!still)
                    continue;
                for (k = 0; k < 3; k++)
                    samples[k][ns] = xb->mv[3][k] - xc->mv[3][k];
                ns++;
            }
        for (k = 0; k < 3; k++)
            pv->de[k] = ns ? median(samples[k], ns) : 0.0f;
        if (pcgfx_cfg.verbose > 2)   /* RUSH_GFX_VERBOSE=3: every transform of the view */
            for (i = 0; i < c->n_xf; i++) {
                Xf *xc = &c->xf[i], *xb = xc->match >= 0 ? &b->xf[xc->match] : NULL;
                int still = 0;
                if (xc->view != j)
                    continue;
                if (xb)
                    rotation_jumped(xb->mv, xc->mv, &still);
                fprintf(stderr, "gfx:     xf %u: %u loads%s%s%s, t %.1f %.1f %.1f", i, xc->nload,
                        xc->has_dyn ? " dyn" : "", xc->persp ? "" : " 2d", still ? " still" : "", xc->mv[3][0],
                        xc->mv[3][1], xc->mv[3][2]);
                if (xb)
                    fprintf(stderr, ", from %.1f %.1f %.1f", xb->mv[3][0], xb->mv[3][1], xb->mv[3][2]);
                fprintf(stderr, "\n");
            }
    }

    /* 3. Several of one signature (instances of a model: a car's wheels, smoke puffs). Under a parent that is
     *    matched, the k-th instance of the parent pairs with the k-th of the parent's partner (a car's wheels come
     *    in its order). The rest pair as mutual nearest neighbours, by where each is against the camera, the
     *    older frame's moved by the camera's move (they stay put in the world). */
    /* where each transform is against its camera, once (the nearest-neighbour search below compares every pair of
     * a model's instances) */
    eb = (float(*)[3])malloc(sizeof(*eb) * (b->n_xf + 1));
    ec = (float(*)[3])malloc(sizeof(*ec) * (c->n_xf + 1));
    for (i = 0; eb != NULL && i < b->n_xf; i++)
        eye_pos(&b->xf[i], eb[i]);
    for (i = 0; ec != NULL && i < c->n_xf; i++)
        eye_pos(&c->xf[i], ec[i]);
    for (i = 0, j = 0; i < c->n_xf && eb != NULL && ec != NULL;) {
        uint32_t i2 = i, j2, a, q;
        while (i2 < c->n_xf && sc[i2].sig == sc[i].sig)
            i2++;
        while (j < b->n_xf && sb[j].sig < sc[i].sig)
            j++;
        j2 = j;
        while (j2 < b->n_xf && sb[j2].sig == sc[i].sig)
            j2++;
        if (j2 > j && !(i2 - i == 1 && j2 - j == 1)) {
            uint8_t *taken = (uint8_t *)calloc(j2 - j, 1);
            if (taken == NULL)
                break;
            for (a = i; a < i2; a++) {   /* by parent */
                Xf *xc = &c->xf[sc[a].idx];
                uint32_t ord = 0, r, k = 0;
                int32_t pb;
                if (xc->parent < 0 || c->xf[xc->parent].match < 0)
                    continue;
                pb = c->xf[xc->parent].match;
                for (r = i; r < a; r++)
                    ord += c->xf[sc[r].idx].parent == xc->parent;
                for (q = j; q < j2; q++)
                    if (b->xf[sb[q].idx].parent == pb && k++ == ord) {
                        if (!taken[q - j] && b->xf[sb[q].idx].view == xc->view) {
                            xc->match = (int32_t)sb[q].idx;
                            taken[q - j] = 1;
                        }
                        break;
                    }
            }
            for (a = i; a < i2; a++) {   /* the rest: mutual nearest */
                Xf *xc = &c->xf[sc[a].idx];
                const float *de;
                float best = 1e30f;
                int32_t bq = -1;
                uint32_t r;
                int mutual = 1;
                if (xc->match >= 0 || xc->view >= nv)
                    continue;
                de = H.pv[xc->view].de;
                for (q = j; q < j2; q++) {
                    Xf *xb = &b->xf[sb[q].idx];
                    float d2;
                    if (taken[q - j] || xb->view != xc->view)
                        continue;
                    d2 = moved_dist2(eb[sb[q].idx], ec[sc[a].idx], de);
                    if (d2 < best)
                        best = d2, bq = (int32_t)q;
                }
                if (bq < 0)
                    continue;
                for (r = i; r < i2 && mutual; r++) {   /* no other of C's is nearer to that one of B's */
                    Xf *xo = &c->xf[sc[r].idx];
                    if (r != a && xo->match < 0 && xo->view == xc->view &&
                        moved_dist2(eb[sb[bq].idx], ec[sc[r].idx], de) < best)
                        mutual = 0;
                }
                if (mutual) {
                    xc->match = (int32_t)sb[bq].idx;
                    taken[bq - j] = 1;
                }
            }
            free(taken);
        }
        i = i2;
        j = j2;
    }
    for (j = 0; j < 3; j++)
        free(samples[j]);
    free(sb);
    free(sc);
    free(eb);
    free(ec);

    /* 4. Per view: a cut? Per transform: motion, or a jump? */
    for (j = 0; j < nv; j++) {
        PView *pv = &H.pv[j];
        uint32_t matched = 0, jumped = 0, nd = 0;
        float *dist = (float *)malloc(sizeof(float) * (c->n_xf + 1)), dmed, de;
        if (j >= b->n_view || !b->view[j].seen || !c->view[j].seen || !c->view[j].persp || !b->view[j].persp ||
            dist == NULL) {
            pv->cut = 1;
            free(dist);
            continue;
        }
        pv->matched = 1;
        if (projection_jumped(b->view[j].proj, c->view[j].proj))
            pv->cut = 1;
        for (i = 0; i < c->n_xf; i++) {
            Xf *xc = &c->xf[i], *xb;
            float e[3], d[3], dl, lim;
            int still;
            if (xc->view != j || !xc->persp || !xc->decomposed)
                continue;
            eye_pos(xc, e);
            dist[nd++] = len3(e);
            if (xc->match < 0)
                continue;
            xb = &b->xf[xc->match];
            if (!xb->persp || !xb->decomposed) {
                xc->match = -1;
                continue;
            }
            matched++;
            d[0] = xb->mv[3][0] - xc->mv[3][0] - pv->de[0];
            d[1] = xb->mv[3][1] - xc->mv[3][1] - pv->de[1];
            d[2] = xb->mv[3][2] - xc->mv[3][2] - pv->de[2];
            dl = len3(d);
            lim = JUMP_MOVE_PER_RETRACE * period;
            if (lim < JUMP_MOVE_REL * len3(e))
                lim = JUMP_MOVE_REL * len3(e);
            if (dl > lim) {
                xc->match = -1;   /* a teleport: where C has it, through the blended camera */
                jumped++;
            } else if (rotation_jumped(xb->mv, xc->mv, &still)) {
                xc->spin = 1;     /* turned too far to blend (a wheel at speed): C's turn, moved part of the way */
            }
            if (pcgfx_cfg.verbose > 2 && (dl > lim || xc->spin))
                fprintf(stderr, "gfx:     xf %u %s: moved %.1f against the camera (limit %.1f)\n", i,
                        dl > lim ? "jumped" : "spun", dl, lim);
        }
        dmed = median(dist, nd);
        free(dist);
        de = len3(pv->de);
        if (de > CUT_MOVE_PER_RETRACE * period || (nd > 0 && de > CUT_MOVE_REL * dmed))
            pv->cut = 1;
        if (jumped > 2 && jumped > CUT_VOTE * matched)
            pv->cut = 1;
        if (matched == jumped)   /* nothing it shows is in both: no evidence the camera moved smoothly */
            pv->cut = 1;
    }

    /* 5. Modes. */
    H.moving = 0;
    for (i = 0; i < c->n_xf; i++) {
        Xf *xc = &c->xf[i];
        if (!xc->persp || !xc->decomposed || xc->view >= nv || H.pv[xc->view].cut)
            continue;
        if (xc->match >= 0) {
            Xf *xb = &b->xf[xc->match];
            xc->mode = XF_LERP;
            if (xc->has_dyn && memcmp(xb->mv, xc->mv, sizeof(xc->mv)) == 0 && len3(H.pv[xc->view].de) > 1e-3f)
                xc->eye_fixed = 1;   /* the camera moved and this didn't: its native vertices are eye-relative */
        } else {
            xc->mode = XF_CAMERA;
        }
        H.moving++;
    }
    if (pcgfx_cfg.verbose) {   /* RUSH_GFX_VERBOSE: each pair the frames between are drawn for */
        uint32_t n[3] = {0, 0, 0}, eye = 0, cuts = 0;
        for (i = 0; i < c->n_xf; i++) {
            n[c->xf[i].mode]++;
            eye += c->xf[i].eye_fixed;
        }
        for (j = 0; j < nv; j++)
            cuts += H.pv[j].matched && H.pv[j].cut;
        fprintf(stderr, "gfx: frames %u -> %u (shown at retrace %lld): %u transforms: %u blended, %u through the "
                "camera (%u eye-fixed), %u as drawn; %u of %u views cut\n", b->seq, c->seq, (long long)b->shown,
                c->n_xf, n[XF_LERP], n[XF_CAMERA], eye, n[XF_SNAP], cuts, nv);
        for (j = 0; j < nv && pcgfx_cfg.verbose > 1; j++)
            if (H.pv[j].matched)
                fprintf(stderr, "gfx:   view %u: camera moved %.1f %.1f %.1f%s\n", j, H.pv[j].de[0], H.pv[j].de[1],
                        H.pv[j].de[2], H.pv[j].cut ? " (cut)" : "");
    }
}

/* ---------------------------------------------------------------- drawing a frame between two */

static void flush(RFrame *c) {
    GfxDrawState st;
    if (H.n_batch > 0 && H.batch_state >= 0) {
        st = c->st[H.batch_state].st;
        st.target = H.aux;
        pcgfx_gl_draw(&st, H.batch, (int)H.n_batch);
    }
    H.n_batch = 0;
}

static void emit(RFrame *c, int state, const GfxOutVtx *v, int n) {
    if (state != H.batch_state || H.n_batch + (uint32_t)n > 3 * 2048)
        flush(c);
    H.batch_state = state;
    if (grow((void **)&H.batch, &H.c_batch, H.n_batch + (uint32_t)n, sizeof(*H.batch)))
        return;
    memcpy(&H.batch[H.n_batch], v, sizeof(*v) * (size_t)n);
    H.n_batch += (uint32_t)n;
}

/* view_x for a viewport between two (gfx_rsp.c's, from the recorded widening) */
static float view_x_at(const RCtx *rc, const float scale[3], const float trans[3], float *xs) {
    float t = trans[0], h, l, r;
    *xs = 1.0f;
    if (rc->ox == 0)
        return t;
    if (!rc->persp)
        return t + (float)rc->ox;
    h = fabsf(scale[0]);
    l = pcgfx_wide_x_at(t - h, rc->frame_w, rc->ox);
    r = pcgfx_wide_x_at(t + h, rc->frame_w, rc->ox);
    if (h > 0.0f)
        *xs = 2.0f * h / (r - l);
    return 0.5f * (l + r);
}

static void draw_between(RFrame *b, RFrame *c, float a) {
    uint32_t i, k, nv = H.n_pv;
    float pa[4][4];
    PcGfxHfrStats *s = &H.stats;
    int vp_lerp;

    s->xf = c->n_xf;
    s->xf_lerp = s->xf_camera = s->xf_snap = s->cuts = 0;
    for (k = 0; k < nv; k++) {
        PView *pv = &H.pv[k];
        if (pv->cut || !pv->matched)
            memcpy(pv->proj, c->view[k].proj, sizeof(pv->proj));
        else
            mtx_lerp(pv->proj, b->view[k].proj, c->view[k].proj, a);
        s->cuts += pv->cut && pv->matched;
    }
    for (i = 0; i < c->n_xf; i++) {
        Xf *x = &c->xf[i];
        float mv[4][4];
        if (x->mode == XF_SNAP) {
            s->xf_snap++;
            continue;
        }
        if (x->mode == XF_LERP) {
            mtx_lerp(mv, b->xf[x->match].mv, x->mv, a);
            if (x->spin)   /* C's turn, at the blended place */
                for (k = 0; k < 3; k++)
                    memcpy(mv[k], x->mv[k], sizeof(mv[k]));
            s->xf_lerp++;
        } else {
            memcpy(mv, x->mv, sizeof(mv));
            for (k = 0; k < 3; k++)
                mv[3][k] += (1.0f - a) * H.pv[x->view].de[k];
            s->xf_camera++;
        }
        memcpy(pa, H.pv[x->view].proj, sizeof(pa));
        mtx_mul(x->now, mv, pa);
        if (x->eye_fixed) {
            memcpy(mv, x->mv, sizeof(mv));
            for (k = 0; k < 3; k++)
                mv[3][k] += (1.0f - a) * H.pv[x->view].de[k];
            mtx_mul(x->eye, mv, pa);
        }
    }

    /* the vertices at this moment */
    if (grow((void **)&H.gv, &H.c_gv, c->n_vtx + 1, sizeof(*H.gv)))
        return;
    for (i = 0; i < c->n_vtx; i++) {
        const RVtx *r = &c->vtx[i];
        const Xf *x = &c->xf[r->xf];
        GfxVtx *g = &H.gv[i];
        g->r = r->rgba[0];
        g->g = r->rgba[1];
        g->b = r->rgba[2];
        g->a = r->rgba[3];
        g->s = r->s;
        g->t = r->t;
        if (x->mode == XF_SNAP || (r->flags & V_FIXED)) {
            g->x = r->clip[0];
            g->y = r->clip[1];
            g->z = r->clip[2];
            g->w = r->clip[3];
        } else {
            const float(*mp)[4] = (x->eye_fixed && (r->flags & V_DYN)) ? x->eye : x->now;
            float ox = r->o[0], oy = r->o[1], oz = r->o[2];
            g->x = ox * mp[0][0] + oy * mp[1][0] + oz * mp[2][0] + mp[3][0];
            g->y = ox * mp[0][1] + oy * mp[1][1] + oz * mp[2][1] + mp[3][1];
            g->z = ox * mp[0][2] + oy * mp[1][2] + oz * mp[2][2] + mp[3][2];
            g->w = ox * mp[0][3] + oy * mp[1][3] + oz * mp[2][3] + mp[3][3];
            if (r->flags & V_FOG)
                g->a = pcgfx_fog_alpha(g->z, g->w, x->fog_mul, x->fog_off);
        }
        g->clip = pcgfx_outcode(g, x->xs);
    }

    /* the frame, op by op */
    H.n_batch = 0;
    H.batch_state = -1;
    vp_lerp = 0;
    for (i = 0; i < c->n_op; i++) {
        const ROp *o = &c->op[i];
        if (o->kind == OP_CLEAR) {
            flush(c);
            pcgfx_gl_clear(H.aux, o->rect, o->depth, o->rgba, o->z);
        } else if (o->kind == OP_VERTS) {
            emit(c, (int)o->state, &c->out[o->first], (int)o->count);
        } else {
            const RState *st = &c->st[o->state];
            uint32_t last_ctx = NONE;
            GfxTriCtx tc;
            for (k = o->first; k < o->first + o->count; k++) {
                const RTri *t = &c->tri[k];
                GfxVtx in[3], poly[8];
                GfxOutVtx out[GFX_TRI_OUT_MAX];
                int n, m;
                if (t->ctx != last_ctx) {
                    const RCtx *rc = &c->ctx[t->ctx];
                    tc = rc->c;
                    last_ctx = t->ctx;
                    /* a viewport that moved a little between the two (a menu's zoom) moves with the rest */
                    if (rc->vp > 0 && rc->vp <= b->n_vp && rc->vp <= c->n_vp && rc->persp) {
                        const RVp *vb = &b->vp[rc->vp - 1], *vc = &c->vp[rc->vp - 1];
                        if (memcmp(vb, vc, sizeof(*vb)) != 0 && fabsf(vb->trans[0] - vc->trans[0]) < 0.25f * rc->c.tw &&
                            fabsf(vb->trans[1] - vc->trans[1]) < 0.25f * rc->c.th &&
                            fabsf(vb->scale[0]) < 2.0f * fabsf(vc->scale[0]) &&
                            fabsf(vc->scale[0]) < 2.0f * fabsf(vb->scale[0])) {
                            int q;
                            for (q = 0; q < 3; q++) {
                                tc.vp_scale[q] = vb->scale[q] + (vc->scale[q] - vb->scale[q]) * a;
                                tc.vp_trans[q] = vb->trans[q] + (vc->trans[q] - vb->trans[q]) * a;
                            }
                            tc.vx = view_x_at(rc, tc.vp_scale, tc.vp_trans, &tc.xs);
                            vp_lerp++;
                        }
                    }
                }
                in[0] = H.gv[t->v[0]];
                in[1] = H.gv[t->v[1]];
                in[2] = H.gv[t->v[2]];
                n = pcgfx_tri_visible(in, &tc, poly);
                if (n == 0)
                    continue;
                m = pcgfx_tri_output(poly, n, &in[0], &tc, &st->st, st->units, out);
                emit(c, (int)o->state, out, m);
            }
        }
    }
    flush(c);
    (void)vp_lerp;
}

/* ---------------------------------------------------------------- presentation */

static RFrame *find_frame(const GfxTarget *t, uint32_t after) {
    RFrame *best = NULL;
    int i;
    for (i = 0; i < HFR_FRAMES; i++) {
        RFrame *f = &H.f[i];
        if (f->used && f != (RFrame *)pcgfx_rec && f->seq > after && f->target == t && (best == NULL || f->seq > best->seq))
            best = f;
    }
    return best;
}

void pcgfx_hfr_presented(GfxTarget *t, uint32_t vi) {
    RFrame *f = find_frame(t, H.cur != NULL ? H.cur->seq : 0);
    if (f == NULL) {
        if (pcgfx_cfg.verbose > 1)
            fprintf(stderr, "gfx: frame swapped in at retrace %u: no recording of it\n", vi);
        if (H.cur == NULL || H.cur->target != t)
            H.cur = NULL, H.prev_shown = -1;
        return;
    }
    H.prev_shown = H.cur != NULL ? H.cur->shown : -1;
    f->shown = vi;
    H.cur = f;
}

/* the frame after cur that is drawn and not shown yet: the oldest such */
static RFrame *next_frame(void) {
    RFrame *best = NULL;
    int i;
    if (H.cur == NULL)
        return NULL;
    for (i = 0; i < HFR_FRAMES; i++) {
        RFrame *f = &H.f[i];
        if (f->used && f != (RFrame *)pcgfx_rec && f->seq > H.cur->seq && f->shown < 0 &&
            (best == NULL || f->seq < best->seq))
            best = f;
    }
    return best;
}

int pcgfx_hfr_frame(double vt, GfxTarget **show, int *rows, int mode) {
    RFrame *b = H.cur, *c, *r, *o;
    double period = 2.0, a;
    int w;
    H.stats.ticks++;
    if (b == NULL || b->shown < 0) {
        H.stats.held++;
        return 0;
    }
    if (H.prev_shown >= 0 && b->shown - H.prev_shown >= 1 && b->shown - H.prev_shown <= 4)
        period = (double)(b->shown - H.prev_shown);
    a = (vt - (double)b->shown) / period;
    c = next_frame();
    H.stats.last_alpha = a;
    if (c == NULL || a <= 0.0) {
        H.stats.held++;
        return 0;
    }
    if (a >= 1.0 || !c->ok || !b->ok || c->n3d == 0 || b->n3d == 0 || b->tw != c->tw || b->th != c->th ||
        b->scale != c->scale) {
        if (a < 1.0) {   /* nothing to move (menus), or the frames differ in kind: as the game shows them */
            H.stats.held++;
            return 0;
        }
        *show = c->target;   /* the next frame is due (the game is late): show it as drawn */
        pcgfx_gl_target_shown(c->target, &w, rows);
        H.stats.early++;
        return 1;
    }
    /* Drawn again from the frame nearer in time, B up to halfway and C after it, with its transforms moved toward
     * the other's: what is in view at that moment is mostly what that frame drew (the road under a camera that
     * moves forward is in B's, not in C's), and its 2D and what appears or goes are that frame's. */
    r = a <= 0.5 ? b : c;
    o = r == b ? c : b;
    if (H.match_r != r->seq || H.match_o != o->seq) {
        match_pair(o, r, (float)period);
        H.match_r = r->seq;
        H.match_o = o->seq;
    }
    if (H.moving == 0) {   /* every view cut, or a new screen: the change comes when the game shows it */
        H.stats.held++;
        return 0;
    }
    /* On the host's clock, drawn only when the GPU has done the game's last frame and the last one drawn here (and not
     * while the host is behind on retraces, mode 2): the
     * frames between take only the time the GPU has to spare, never the game's. The screen keeps what it shows. */
    if (mode == 2 || (mode == 1 && pcgfx_gl_fence_busy())) {
        H.stats.busy++;
        return 3;
    }
    H.aux = pcgfx_gl_aux_target(H.aux, r->tw, r->th, r->scale);
    if (H.aux == NULL)
        return 0;
    pcgfx_gl_timer_begin(PCGFX_GPU_BETWEEN);   /* RUSH_GFX_TIMES=3 */
    draw_between(o, r, (float)(r == c ? a : 1.0 - a));
    pcgfx_gl_timer_end();
    pcgfx_gl_fence_set();
    *show = H.aux;
    pcgfx_gl_target_shown(r->target, &w, rows);
    H.stats.drawn++;
    return 2;
}

/* RUSH_GFX_HFR_DUMP=DIR[:FIRST[:COUNT]]: each frame shown, from tick FIRST on (0), COUNT of them (100), as
 * DIR/hfr-TICK-AAA.png, AAA the fraction of the way to the next frame in hundredths ("real" for a frame as the
 * game drew it). A tool for looking at the frames drawn between two, against the game's own. */
void pcgfx_hfr_dump(GfxTarget *t, int drawn) {
    char path[600], what[8];
    unsigned first = 0, count = 100;
    const char *colon;
    uint8_t *px;
    int w, h, rc;
    uint32_t tick = H.stats.ticks - 1;
    if (H.dump == NULL)
        return;
    colon = strchr(H.dump, ':');
    if (colon != NULL)
        sscanf(colon + 1, "%u:%u", &first, &count);
    if (tick < first || tick >= first + count)
        return;
    rc = t != NULL ? pcgfx_gl_read_target(t, NULL, &w, &h) : pcgfx_gl_read_presented(NULL, &w, &h);
    if (rc != 0 || (px = (uint8_t *)malloc((size_t)w * h * 4)) == NULL)
        return;
    rc = t != NULL ? pcgfx_gl_read_target(t, px, &w, &h) : pcgfx_gl_read_presented(px, &w, &h);
    if (drawn)
        snprintf(what, sizeof(what), "%03d", (int)(H.stats.last_alpha * 100.0 + 0.5));
    else
        snprintf(what, sizeof(what), "real");
    snprintf(path, sizeof(path), "%.*s/hfr-%06u-%s.png", colon != NULL ? (int)(colon - H.dump) : (int)strlen(H.dump),
             H.dump, tick, what);
    if (rc == 0)
        pcgfx_png_write(path, px, w, h);
    free(px);
}

void pcgfx_hfr_reset(void) {
    int i;
    if (pcgfx_rec != NULL)
        pcgfx_rec = NULL;
    for (i = 0; i < HFR_FRAMES; i++) {
        H.f[i].used = 0;
        H.f[i].shown = -1;
    }
    H.cur = NULL;
    H.prev_shown = -1;
    H.dump = getenv("RUSH_GFX_HFR_DUMP");
    if (H.dump != NULL && !*H.dump)
        H.dump = NULL;
}

void pcgfx_hfr_shutdown(void) {
    int i;
    pcgfx_rec = NULL;
    for (i = 0; i < HFR_FRAMES; i++)
        frame_free(&H.f[i]);
    pcgfx_gl_aux_free(H.aux);
    free(H.pv);
    free(H.batch);
    free(H.gv);
    memset(&H, 0, sizeof(H));
}

void pcgfx_hfr_stats(PcGfxHfrStats *s) {
    *s = H.stats;
}
