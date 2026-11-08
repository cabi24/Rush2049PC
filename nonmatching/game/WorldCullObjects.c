/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * best for WorldCullObjects = WorldCullObjects@world.c (MAT3 world.c, WorldDistance inlined).
 * NOT A MATCH: unit `FAIL WorldCullObjects: 32 of 294 words differ; compiled body is 293 words` (best: 52).
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

extern tModelPart *WorldBase;
extern s8 gTrackNum;
extern s16 Worlddlidx;
extern s8 gInvisibleTrack;
extern ObjInstance ObjInstanceList[];    /* ObjInstanceList */
extern u8 trackParts[];             /* trackParts */
extern u32 showAllParts[];            /* showAllParts */
extern u32 track1Parts[][4], track2Parts[][4], track3Parts[][4], track4Parts[][4], track5Parts[][4], track6Parts[][4];
extern u32 track7Parts[][4], track8Parts[][4], track9Parts[][4], track10Parts[][4], track11Parts[][4], track12Parts[][4];
extern u32 track13Parts[][4], track14Parts[][4], track15Parts[][4], track16Parts[][4], track17Parts[][4], track18Parts[][4];
extern u32 track19Parts[][4];

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
                *widx = Wnode - WorldBase;
                *dist = d;
            }
        }
        if (Wnode->next < 0) {
            break;
        }
        (*index)++;
        Wnode = WorldBase + Wnode->next;
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
    case 0: return track1Parts[cell];
    case 1: return track2Parts[cell];
    case 2: return track3Parts[cell];
    case 3: return track4Parts[cell];
    case 4: return track5Parts[cell];
    case 5: return track6Parts[cell];
    case 6: return track7Parts[cell];
    case 7: return track8Parts[cell];
    case 8: return track9Parts[cell];
    case 9: return track10Parts[cell];
    case 10: return track11Parts[cell];
    case 11: return track12Parts[cell];
    case 12: return track13Parts[cell];
    case 13: return track14Parts[cell];
    case 14: return track15Parts[cell];
    case 15: return track16Parts[cell];
    case 16: return track17Parts[cell];
    case 17: return track18Parts[cell];
    case 18: return track19Parts[cell];
    default: return 0;
    }
}

/* Each track's table's rows, as far as it runs before the next one (track1Parts... in address order: the battle
 * arenas' have one, the stunt arena 10's 14): a cell past them has no row of its own. */
static const u8 VisRows[19] = {117, 117, 117, 121, 103, 122, 1, 1, 1, 1, 14, 1, 1, 1, 1, 1, 1, 1, 25};

/* The rows of every cell within reach feet of pos, or'ed into far[4]: the cells in WorldDistance's order, those with
 * a box (as ZOID_BuildCullingList makes a culling box of) and a row of their table. */
static void VisReach(f32 *pos, f32 reach, u32 *far)
{
    tModelPart *Wnode = WorldBase;
    s32 index = 0, k;
    f32 dv[3], d, e;
    u32 *row;

    for (;;) {
        if ((Wnode->min[0] == Wnode->max[0] && Wnode->min[1] == Wnode->max[1]) || gTrackNum < 0 ||
            gTrackNum >= 19 || index >= VisRows[gTrackNum]) {
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
        if (d <= reach * reach && (row = VisRow(gTrackNum, index)) != 0) {
            for (k = 0; k < 4; k++) {
                far[k] |= row[k];
            }
        }
    next:
        if (Wnode->next < 0) {
            break;
        }
        index++;
        Wnode = WorldBase + Wnode->next;
    }
}

void WorldCullObjects(s32 unused, f32 *pos, s32 show)
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
    if (!WorldBase) {
        return;
    }
    WorldDistance(WorldBase, pos, &best, &dist, &windex, &dlindex);
    if (best < 0) {
        ptr = showAllParts;
    } else {
        ptr = VisRow(gTrackNum, best);
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
    parts = trackParts[gTrackNum];
    pc_scene_count(PC_SCENE_PARTS, parts);
    mask = 1;
    w = 0;
    for (i = 0; i < parts; i++) {
        if (i && !(i & 31)) {
            w++;
            mask = 1;
        }
        if (((ptr && (mask & ptr[w])) || show) && !gInvisibleTrack) {
            ObjInstanceList[Worlddlidx + i].flags &= ~0x80000000;
            PC_SCENE(WORLD)
        } else {
            ObjInstanceList[Worlddlidx + i].flags |= 0x80000000;
        }
        mask <<= 1;
    }
}
