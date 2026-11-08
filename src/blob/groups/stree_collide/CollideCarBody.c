/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* CollideCarBody (0x800C6404, 1468 B): per-car wheel collision against the track's polygon quadtree (the
 * label is historical). For each of the 4 wheels, floor the (x, z) position and look up its quadtree leaf
 * (updownleaf); collect the distinct (leaf, quadrant) cells; unpack each cell's packed polygon list
 * (DecodeSlist, the same `n = *p++; DecodeSlist(p, n, ...)` idiom as RoadPosition/TireSurf)
 * and run CollideCarBodyAgainstSurface on every new non-wall polygon (0x4000 marks visited). Pending contact -> CarApplyCollision.
 * Then for wall polygons (0x2000) test each wheel (PointInSurf); on a hit project it (fHalfPipeInterp)
 * and, when the wheel moved into the wall, push it out (CarApplyCollision). Finally pos -> old for the 4 wheels.
 * first draft; matched it in the whole-program unit (EQUAL, 367 words) with PointInSurf's real
 * register-parameter convention. No unused locals, no compiled-out reads, no stand-ins.
 * Source facts that decide the code (each traced):
 *   - loop 2's inner test is written with `continue`: the CollideCarBodyAgainstSurface call block is then the
 *     branch-taken successor, so uopt's DFS livbb order puts the C36A0/C3AD0 call blocks first and car's
 *     live range is split there (car stays in s4 across the loops, reloaded after each IPA call, as retail);
 *   - one counter `j` for loop 1, loop 2 and loop 4 (loop 1's exit test stays `slti`; j gets a1 in loops 2/4),
 *     `i` for the search, loop-2 inner and loop-4 inner loops, `k` for the (unrolled) final copy loop;
 *   - `bound` (address passed to PointInSurf) is tested through the variable after `bound = pt[1]`;
 *   - `u8 cnt` declared after `s16 out` (fills its pad) and `u8 *q` (frame 584 with retail's slot homes). */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)

typedef struct QNode {
    s16 next;
    u8 pad2;
    u8 mask;
    s16 x0;
    s16 x1;
    s16 y0;
    s16 y1;
    u16 child[4];
} QNode;

typedef struct Poly {
    u16 type;
    u16 cnt;
    u8 body[0x12];
    u16 off;
} Poly;

typedef struct CCol {
    Poly *poly;
    f32 m[9];
    s16 idx;
    s16 idx2;
    f32 k;
} CCol;

/* PC: the car's model record (D_8014A250) keeps its N64 layout, and its tires' road nodes are u32 window addresses
 * there (docs/port/CARS.md), as TireSurf.c's view has them. */
enum n64p_QNode { n64p_QNode_ };
typedef struct VCar {
    u8 p0[628];
    f32 pos[4][3];      /* 628: wheel positions */
    f32 old[4][3];      /* 676: previous wheel positions */
    u8 p724[204];
    enum n64p_QNode node[4];     /* 928 */
} VCar;
#define N64P(x) _Generic((x), enum n64p_QNode: (QNode *)PC_N64P(x), default: (x))

extern Poly *stree_surf;
extern u8 *stree_slist;
extern s8 tireIndex[];

QNode *updownleaf(QNode *n, s16 x, s16 y, s16 *out);
void DecodeSlist(u8 *input, s32 count, u16 *front, u32 marker);
void CollideCarBodyAgainstSurface(void *car, Poly *poly, CCol *col);
s16 PointInSurf(Poly *poly, f32 *wp, f32 *q, f32 *pt, f32 *bound, s16 *outIdx, f32 *mat, f32 zmin);
void fHalfPipeInterp(s32 arg0, u16 idx, f32 *position, f32 *outPosition, f32 (*outMatrix)[3], f32 threshold);
void BodyVector(f32 *in, f32 *out, f32 *mat);
void CarApplyCollision(void *car, CCol *col);

#define FLOOR(v) (((v) < 0.0f && (f32)(s32)(v) != (v)) ? (v) - 1.0f : (v))

void CollideCarBody(VCar *car)
{
    QNode *cells[4];
    s16 cellIdx[4];
    u16 polys[142];
    s32 npoly;
    s32 ncells = 0;
    s32 j;
    s32 i;
    s32 k;
    s16 out;
    u8 cnt;
    CCol col = { 0 };
    s32 unique;
    u32 off;
    u8 *q;
    Poly *p;
    Poly *poly;
    f32 bound;
    f32 pt[3];
    f32 w[3];
    s16 outIdx;

    for (j = 0; j < 4; j++) {
        p = (Poly *)updownleaf(N64P(car->node[j]), (s32)FLOOR(car->pos[j][0]), (s32)FLOOR(car->pos[j][2]), &out);
        if (p != NULL) {
            unique = 1;
            for (i = ncells - 1; i >= 0; i--) {
                if ((QNode *)p == cells[i] && out == cellIdx[i]) {
                    unique = 0;
                    break;
                }
            }
            if (unique) {
                cellIdx[ncells] = out;
                cells[ncells] = (QNode *)p;
                ncells++;
            }
        }
    }
    npoly = 0;
    for (j = 0; j < ncells; j++) {
        off = cells[j]->child[cellIdx[j]];
        if (cells[j]->mask & (16 << cellIdx[j])) {
            off |= 0x10000;
        }
        if (off != 0) {
            q = stree_slist + off;
            cnt = *q++;
            DecodeSlist(q, cnt, &polys[npoly], 0);
            for (i = npoly; i < cnt + npoly; i++) {
                p = &stree_surf[polys[i]];
                if (col.poly == p || (p->type & 0x4000) || (p->type & 0x2000) || (p->type & 0xF) == 15) {
                    continue;
                }
                CollideCarBodyAgainstSurface(car, p, &col);
                p->type |= 0x4000;
            }
            npoly += cnt;
        }
    }
    if (col.poly != NULL) {
        CarApplyCollision(car, &col);
    }
    for (j = 0; j < npoly; j++) {
        poly = &stree_surf[polys[j]];
        poly->type &= ~0x4000;
        if (poly->type & 0x2000) {
            for (i = 0; i < 4; i++) {
                bound = 250.0f;
                if (PointInSurf(poly, car->pos[tireIndex[i]], car->old[tireIndex[i]], pt, &bound, &outIdx, col.m, -5.0f) != 0) {
                    fHalfPipeInterp((intptr_t)poly, outIdx, car->pos[i], pt, (f32 (*)[3])col.m, 0.0f);
                    bound = pt[1];
                    if (bound < -5.0f || bound >= 0.0f) {
                        continue;
                    }
                    pt[0] = car->pos[tireIndex[i]][0] - car->old[tireIndex[i]][0];
                    pt[1] = car->pos[tireIndex[i]][1] - car->old[tireIndex[i]][1];
                    pt[2] = car->pos[tireIndex[i]][2] - car->old[tireIndex[i]][2];
                    BodyVector(pt, w, col.m);
                    if (w[1] >= 0.0f) {
                        continue;
                    }
                    col.poly = poly;
                    col.idx = i;
                    col.idx2 = i;
                    col.k = -bound;
                    CarApplyCollision(car, &col);
                }
            }
        }
    }
    for (k = 0; k < 4; k++) {
        car->old[k][0] = car->pos[k][0];
        car->old[k][1] = car->pos[k][1];
        car->old[k][2] = car->pos[k][2];
    }
}
