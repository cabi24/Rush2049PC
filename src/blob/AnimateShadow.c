/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* AnimateShadow = AnimateShadow (car shadow visual). MATCH (standalone -O3 and whole-program unit).
 * GC identity: MAT3 GameSource/src/visuals.c static AnimateShadow (GC 0x80059578, 0x99C), same flow as the
 * arcade AnimateShadow (reference/repos/rushtherock/game/visuals.c): ZOID_RemovePoly on op 0, airdist diagonal
 * test -> ZOID_HidePoly, 0,1,3,2 wheel order, width then front/back scale loops (shadow_scale[body][0..3],
 * body 13 when appearance & 0x10), ZOID_ShowPoly, alpha 255 - clamp(8*d1, 20, 255) capped at 192 (gShadowColor),
 * then ZOID_ClearPolyFlags(poly, 1 << car->camera) when camera >= 0 and view is 0 or 1, else
 * ZOID_SetPolyFlags(poly, 15), and ZOID_UpdatePoly. The N64 has none of the GC dr_uvs swap/lean code, the
 * BodyVector hide test or the packed-shadow copy.
 * lever (from MAT3): the four poly-flag helpers are zoidpoly.c functions that the whole-program build
 * inlines (N64 out-of-line copies: ZOID_ClearPolyFlags = ZOID_ClearPolyFlags, ZOID_SetPolyFlags = ZOID_SetPolyFlags,
 * ZOID_ShowPoly = ZOID_ShowPoly); here they are byte-equivalent static copies. Passing `1 << car->camera` as
 * the u16 argument (camera read twice, as GC does) gives retail's evaluation order shift, load, NOT, which no
 * single-expression form gave (2 words). Each inlined call reserves 8 frame bytes, which replaces
 * most of the earlier filler locals; the `scale` row pointer and its DEBUG_PRINT holder are gone (GC indexing).
 * Lineage: best.c (2 words) -> .
 * Unused locals (exact frame residual, disclosed): pos[3], mat[3][3], d2 declared before/among the used locals.
 * Retail frame 280; without them 224 (52 bytes of slots). GC declares pos[3], tvec[3] before xyz; the N64
 * needs 48 bytes there.
 * Shaping kept from earlier (each measured): int literal 20 in the four fabsf tests; offset/fscale/bscale
 * reused for the airdist differences (one variable = one register; GC reuses d2/d3/d4 the same way).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

typedef struct Visual {
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
    /* 0x00 */ void *pad0;
    /* 0x04 */ u8 pad4[2];
    /* 0x06 */ s16 objnum;
    /* 0x08 */ s16 slot;
    /* 0x0A */ u8 padA[0x14 - 0x0A];
    /* 0x14 */ void (*func)(struct Visual *v, s16 op);
} Visual;

typedef struct {
    /* 0x00 */ s16 active;
    /* 0x02 */ u16 flags;
    /* 0x04 */ u8 pad[0x54];
} Poly;

typedef struct {
    /* 0x000 */ u8 pad0[0x74];
    /* 0x074 */ f32 dr_tirepos[4][3];
    /* 0x0A4 */ u8 padA4[0xE8 - 0xA4];
    /* 0x0E8 */ u32 flags;
    /* 0x0EC */ u8 padEC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 padEC_308[0x35C - 0x308];
    /* 0x35C */ s8 unk35C;
    /* 0x35D */ s8 unk35D;
    /* 0x35E */ s8 unk35E;
    /* 0x35F */ s8 unk35F;
    /* 0x360 */ u8 pad360[0x380 - 0x360]; void *input; u8 pad360_384[0x3B8 - 0x384];
} CarData;

typedef struct {
    /* 0x000 */ u8 pad0[8];
    /* 0x008 */ u8 body_type;
    /* 0x009 */ u8 pad9[0x6C4 - 9];
    /* 0x6C4 */ s16 moving_state;
    /* 0x6C6 */ u8 pad6C6[0x76C - 0x6C6];
    /* 0x76C */ f32 airdist[4];
    /* 0x77C */ u8 pad77C[0x7DF - 0x77C];
    /* 0x7DF */ s8 hide_car;
    /* 0x7E0 */ u8 pad7E0[0x808 - 0x7E0];
} ModelDat;

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Poly PolyInstanceList[];
extern s8 gInvisibleCar;
extern f32 shadow_scales[14][4];   /* shadow_scale: body types 0-13 (13 when appearance & 0x10); text follows */
extern u8 gShadowColor[4];

extern void ZOID_RemovePoly(Poly *p);
extern void ZOID_UpdatePoly(Poly *record, s32 count, f32 (*positions)[3], u16 parameter, u8 *color, u16 flags, s32 indexed);

#define DEBUG_PRINT(x)
#define rng(x, lo, hi) (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

/* The game's own ZOID_HidePoly and ZOID_ShowPoly (this file had static copies: the same code). */
void ZOID_HidePoly(Poly *poly);
void ZOID_ShowPoly(Poly *poly);

void ZOID_ClearPolyFlags(Poly *poly, unsigned short flags);

void ZOID_SetPolyFlags(Poly *poly, unsigned short flags);

void AnimateShadow(Visual *v, s16 op) {
    f32 pos[3], mat[3][3], xyz[4][3], dist[4], d1, d2;
    s16 i, whl;
    CarData *car = PC_CAR(v->slot);
    ModelDat *m = PC_MODEL(v->slot);
    f32 fscale, bscale, len, offset;
    s32 body;

    if (op == 0) {
        if (v->objnum >= 0) {
            ZOID_RemovePoly(&PolyInstanceList[v->objnum]);
        }
        v->func = 0;
        v->objnum = -1;
        return;
    }

    d1 = m->airdist[3] - m->airdist[1];
    offset = m->airdist[2] - m->airdist[0];
    fscale = m->airdist[1] - m->airdist[0];
    bscale = m->airdist[3] - m->airdist[2];
    if (fabsf(d1) > 20 || fabsf(offset) > 20 || fabsf(fscale) > 20 || fabsf(bscale) > 20 ||
        car->unk35F != 0 || m->moving_state > -1 || (car->flags & 8) || gInvisibleCar != 0 || m->hide_car) {
        ZOID_HidePoly(&PolyInstanceList[v->objnum]);
        return;
    }

    for (d1 = i = 0; i < 4; i++) {
        d1 += (dist[i] = m->airdist[i]);
        if (dist[i] < 0.0f) {
            dist[i] = 0.0f;
        }
    }
    d1 *= 0.25f;

    for (i = 0; i < 4; i++) {
        whl = (i < 2) ? i : 5 - i;
        offset = (dist[whl] > 10.0f) ? 2.0f : dist[whl] * 0.1f + 1.0f;
        xyz[i][0] = car->dr_tirepos[whl][0];
        xyz[i][1] = car->dr_tirepos[whl][1] - dist[whl] + offset;
        xyz[i][2] = car->dr_tirepos[whl][2];
    }

    if (car->flags & 0x10) {
        body = 13;
    } else {
        body = m->body_type;
    }

    fscale = shadow_scales[body][0];
    bscale = shadow_scales[body][1];
    for (i = 0; i < 3; i++) {
        len = xyz[0][i] - xyz[1][i];
        xyz[0][i] += len * fscale;
        xyz[1][i] -= len * fscale;

        len = xyz[3][i] - xyz[2][i];
        xyz[3][i] += len * bscale;
        xyz[2][i] -= len * bscale;
    }

    fscale = shadow_scales[body][2];
    bscale = shadow_scales[body][3];
    for (i = 0; i < 3; i++) {
        len = xyz[0][i] - xyz[3][i];
        xyz[0][i] += len * fscale;
        xyz[3][i] -= len * bscale;

        len = xyz[1][i] - xyz[2][i];
        xyz[1][i] += len * fscale;
        xyz[2][i] -= len * bscale;
    }

    ZOID_ShowPoly(&PolyInstanceList[v->objnum]);
    gShadowColor[3] = 255.0f - rng(d1 * 8.0f, 20.0f, 255.0f);
    if (gShadowColor[3] > 192) {
        gShadowColor[3] = 192;
    }

    if (car->unk35C >= 0 && (car->unk35D == 0 || car->unk35D == 1))
        ZOID_ClearPolyFlags(&PolyInstanceList[v->objnum], 1 << car->unk35C);
    else ZOID_SetPolyFlags(&PolyInstanceList[v->objnum], 15);
    ZOID_UpdatePoly(&PolyInstanceList[v->objnum], 4, xyz, 0, gShadowColor, 0, 0);
}
