/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * best for physics_float_calc = WorldCullObjects@world.c (MAT3 world.c, WorldDistance inlined).
 * NOT A MATCH: unit `FAIL physics_float_calc: 32 of 294 words differ; compiled body is 293 words` (best: 52).
 * Built on f7 (best + `&&` spelling of the dead ptr_no_clip clause + one unused float `e`
 * WorldDistance: frame 136 and every home fp-exact; DISCLOSED shaping, frame 136 with it, 128 without).
 * levers (all compiled-out, disclosed): `if (show) {}` at the top and bottom of the cull loop and `if (parts) {}`
 * after it (they colour t0/t1/t2 as retail, so the ugen ring and every pre-loop row is retail's), and `w++; mask = 1;`
 * (retail order of the `li v0,1`). Residual: show is coloured t0 (retail keeps it in memory) and the loop IVs
 * i*68 / w*4 / ptr+w*4 are a1/a2/a3 (retail a3/a2/t0): retail has an a1 blocker and the w*4 IV numbered first.
 */
typedef float f32;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef signed char s8;
typedef unsigned char u8;

typedef struct {
    char name[16];          /* 0x00 */
    f32 uvs[4][3];          /* 0x10 mat; pos = uvs[3] at 0x34 */
    u32 dlflags;            /* 0x40 */
    s16 next;               /* 0x44 */
    s16 child;              /* 0x46 */
    s16 sfx1;               /* 0x48 */
    s16 sfx2;               /* 0x4A */
    s32 tidx;               /* 0x4C */
    f32 min[3];             /* 0x50 */
    f32 max[3];             /* 0x5C */
} tModelPart; /* 0x68 */

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct {
    u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env;
} ObjInstance;

extern tModelPart *D_80149B80;      /* WorldBase */
extern s8 D_8014978C;               /* gTrackNum */
extern s16 D_80149D90;              /* Worlddlidx */
extern s8 D_8014061A;               /* gInvisibleTrack */
extern ObjInstance D_8012E700[];    /* ObjInstanceList */
extern u8 D_8011E748[];             /* trackParts */
extern u32 D_8011E75C[];            /* showAllParts */
extern u32 D_8011B898[][4], D_8011BFE8[][4], D_8011C738[][4], D_8011CE88[][4], D_8011D618[][4], D_8011DC88[][4];
extern u32 D_8011E428[][4], D_8011E438[][4], D_8011E448[][4], D_8011E458[][4], D_8011E468[][4], D_8011E548[][4];
extern u32 D_8011E558[][4], D_8011E568[][4], D_8011E578[][4], D_8011E588[][4], D_8011E598[][4], D_8011E5A8[][4];
extern u32 D_8011E5B8[][4];

void ShowCull();
float sqrtf(float);
#pragma intrinsic(sqrtf)

static void WorldDistance(tModelPart *Wnode, f32 *pos, s32 *best, f32 *dist, s32 *widx, s32 *index)
{
    f32 dv[3];
    f32 d;
    f32 e;

    for (;;) {
        dv[0] = pos[0] - Wnode->uvs[3][0];
        dv[1] = pos[1] - Wnode->uvs[3][1];
        dv[2] = pos[2] - Wnode->uvs[3][2];

        if (Wnode->min[0] <= dv[0] && dv[0] <= Wnode->max[0] && Wnode->min[1] <= dv[1] && Wnode->min[2] <= dv[2] &&
            dv[2] <= Wnode->max[2]) {
            d = sqrtf(dv[0] * dv[0] + dv[2] * dv[2]);
            if (dv[1] > 0.75f * Wnode->max[1]) {
                d += dv[1] - Wnode->min[1];
            }
            if (*best < 0 || d < *dist) {
                *best = *index;
                *widx = Wnode - D_80149B80;
                *dist = d;
            }
        }
        if (Wnode->next < 0) {
            break;
        }
        (*index)++;
        Wnode = D_80149B80 + Wnode->next;
    }
}

/* PC: video.draw_distance (docs/port/DETAIL.md 2.1). The table row of the part the eye is in (WorldDistance's best)
 * says which of the world's parts are drawn. Far draws the parts any row lets through whose part lies within
 * PC_VIS_FAR feet of the eye (the distance to its box, as WorldDistance places the eye in one): what the parts the
 * eye can reach in that much further draw, PC_DRAW_FAR_FACTOR times the 2,000-ft far plane the rows were made for.
 * Max draws every part, as show does. Original is the N64's. The parts let through are counted (PC_SCENE). */
#define PC_VIS_FAR (2000.0f * (PC_DRAW_FAR_FACTOR - 1.0f))

static u32 *VisRow(s32 track, s32 cell)
{
    switch (track) {
    case 0: return D_8011B898[cell];
    case 1: return D_8011BFE8[cell];
    case 2: return D_8011C738[cell];
    case 3: return D_8011CE88[cell];
    case 4: return D_8011D618[cell];
    case 5: return D_8011DC88[cell];
    case 6: return D_8011E428[cell];
    case 7: return D_8011E438[cell];
    case 8: return D_8011E448[cell];
    case 9: return D_8011E458[cell];
    case 10: return D_8011E468[cell];
    case 11: return D_8011E548[cell];
    case 12: return D_8011E558[cell];
    case 13: return D_8011E568[cell];
    case 14: return D_8011E578[cell];
    case 15: return D_8011E588[cell];
    case 16: return D_8011E598[cell];
    case 17: return D_8011E5A8[cell];
    case 18: return D_8011E5B8[cell];
    default: return 0;
    }
}

/* Each track's table's rows, as far as it runs before the next one (D_8011B898... in address order: the battle
 * arenas' have one, the stunt arena 10's 14): a cell past them has no row of its own. */
static const u8 VisRows[19] = {117, 117, 117, 121, 103, 122, 1, 1, 1, 1, 14, 1, 1, 1, 1, 1, 1, 1, 25};

/* The rows of every cell within reach feet of pos, or'ed into far[4]: the cells in WorldDistance's order, those with
 * a box (as func_800A7E10 makes a culling box of) and a row of their table. */
static void VisReach(f32 *pos, f32 reach, u32 *far)
{
    tModelPart *Wnode = D_80149B80;
    s32 index = 0, k;
    f32 dv[3], d, e;
    u32 *row;

    for (;;) {
        if ((Wnode->min[0] == Wnode->max[0] && Wnode->min[1] == Wnode->max[1]) || D_8014978C < 0 ||
            D_8014978C >= 19 || index >= VisRows[D_8014978C]) {
            goto next;
        }
        d = 0.0f;
        for (k = 0; k < 3; k++) {
            dv[k] = pos[k] - Wnode->uvs[3][k];
            e = 0.0f;
            if (dv[k] < Wnode->min[k]) {
                e = Wnode->min[k] - dv[k];
            } else if (k != 1 && dv[k] > Wnode->max[k]) {   /* no top, as in WorldDistance */
                e = dv[k] - Wnode->max[k];
            }
            d += e * e;
        }
        if (d <= reach * reach && (row = VisRow(D_8014978C, index)) != 0) {
            for (k = 0; k < 4; k++) {
                far[k] |= row[k];
            }
        }
    next:
        if (Wnode->next < 0) {
            break;
        }
        index++;
        Wnode = D_80149B80 + Wnode->next;
    }
}

void physics_float_calc(s32 unused, f32 *pos, s32 show)
{
    s32 dlindex;
    s32 windex;
    f32 dist;
    s32 best;
    s32 parts;
    s32 i;
    u32 mask;
    s32 w;
    u32 *ptr;
    u32 far[4];
    s32 draw = pc_draw_distance();

    dlindex = 0;
    windex = 0;
    best = -1;
    if (!D_80149B80) {
        return;
    }
    WorldDistance(D_80149B80, pos, &best, &dist, &windex, &dlindex);
    if (best < 0) {
        ptr = D_8011E75C;
    } else {
        ptr = VisRow(D_8014978C, best);
    }
    if (draw == PC_DRAW_MAX) {
        show = 1;
    } else if (draw == PC_DRAW_FAR && best >= 0 && ptr) {
        for (i = 0; i < 4; i++) {
            far[i] = ptr[i];
        }
        VisReach(pos, PC_VIS_FAR, far);
        ptr = far;
    }
    parts = D_8011E748[D_8014978C];
    pc_scene_count(PC_SCENE_PARTS, parts);
    mask = 1;
    w = 0;
    for (i = 0; i < parts; i++) {
        if (i && !(i & 31)) {
            w++;
            mask = 1;
        }
        if (((ptr && (mask & ptr[w])) || show) && !D_8014061A) {
            D_8012E700[D_80149D90 + i].flags &= ~0x80000000;
            PC_SCENE(WORLD)
        } else {
            D_8012E700[D_80149D90 + i].flags |= 0x80000000;
        }
        mask <<= 1;
    }
}
