#include "game/game.h"

float sqrtf(float);
#pragma intrinsic (sqrtf)

typedef struct EffectObj {
    /* 0x00 */ struct EffectObj *next;   /* pool list link */
    void *prev;                         /* the list's back link (GetStatLLE), pointer-sized here */
    /* 0x08 */ f32 f8;
    /* 0x0C */ void *dl;                 /* display handle */
    /* 0x10 */ f32 pos[3];
    /* 0x1C */ u8 color[4];
    /* 0x20 */ s32 f20;
} EffectObj;

typedef struct WheelSlot {
    /* 0x00 */ EffectObj *obj;
    /* 0x04 */ f32 start[3];
    /* 0x10 */ f32 pos[3];
    /* 0x1C */ f32 lenSq;
    /* 0x20 */ f32 dir[3];
    /* 0x2C */ f32 quad[6];             /* 0x2C..0x40: previous edge */
    /* 0x44 */ f32 left[3];
    /* 0x50 */ f32 right[3];
} WheelSlot;                            /* 0x5C */

typedef struct CarState {
    /* 0x000 */ u8 pad0[0x5A0];
    /* 0x5A0 */ u16 surface[4];
    /* 0x5A8 */ u8 pad5A8[0x61C - 0x5A8];
    /* 0x61C */ u16 terrain[4];
    /* 0x624 */ u8 pad624[0x6C4 - 0x624];
    /* 0x6C4 */ s16 airborne[4];
    /* 0x6CC */ u8 pad6CC[0x75C - 0x6CC];
    /* 0x75C */ f32 wheelHeight[4];
    /* 0x76C */ u8 pad76C[0x808 - 0x76C];
} CarState;                             /* 0x808 */

typedef struct Surface {
    /* 0x0 */ u8 pad0[2];
    /* 0x2 */ u16 flags;
    /* 0x4 */ u8 pad4[0x18 - 4];
} Surface;                              /* 0x18 */

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the skid state is pc_skids, PC_MAX_LINKS cars of four tires, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6), with WheelSlot's field names. */
#define gNewSkid pc_skids

VIEW(s32, gGameState);
#define gGameState gGameState_view
VIEW(u32[4], skid_tire_flags);
#define skid_tire_flags skid_tire_flags_view

VIEW(u8[], gShadowColor);
#define gShadowColor gShadowColor_view
extern f32 gDummyVtx[];
VIEW(Surface *, stree_surf);
#define stree_surf stree_surf_view

VIEW(f32[], gSeconds);
#define gSeconds gSeconds_view
VIEW(char[], gSkidList);
#define gSkidList gSkidList_view
#define POOL_HEAD 3   /* the active list, read as the pool header's pointer word: host +0x18 */
typedef struct {
    /* 0x00 */ f32 uvs[3][3];
    /* 0x24 */ f32 pos[3];
} CamXform;
VIEW(CamXform, gCamera);
#define gCamera gCamera_view

extern void ZOID_RemovePoly(void *dl);
extern void *ZOID_NewPoly(s32, f32 *, u16, u8 *, s32, s32);
extern void ZOID_UpdatePoly(void *dl, s32 n, f32 *quad, s32, u8 *color, s32, s32);

/* UpdateSkid (0x800AF8C0, 113 words) = arcade UpdateSkid: place the skid's leading edge across the
 * axle (tires tire|1 and tire&2), 0.75 either side of the tire, lifted 0.8; stamp time and colour; update
 * the polygon.  IPA parameters: ns $s0, slot $t1, tire $s4, color $s5.
 * Quirk for strictness: the 0.8f lift is this function's own .rodata (0x80123C08).  It is read through the
 * extern into a local `up` declared above the vectors (same code as the natural `+= 0.8f`, which scores
 * "2 relocations unverified"); `car` must stay unnamed and `m` named for the frame and registers.
 */
void UpdateSkid(WheelSlot *slot, s16 player, u32 wheel, u8 *color)
{
    CarState *m = PC_MODEL(player);
    EffectObj *obj = slot->obj;
    s16 i;
    f32 up;
    f32 orth[3];
    f32 dir[3];
    f32 wid;

    orth[0] = PC_CAR_WHEEL_POS(player)[wheel | 1][0] - PC_CAR_WHEEL_POS(player)[wheel & 2][0];
    orth[1] = PC_CAR_WHEEL_POS(player)[wheel | 1][1] - PC_CAR_WHEEL_POS(player)[wheel & 2][1];
    orth[2] = PC_CAR_WHEEL_POS(player)[wheel | 1][2] - PC_CAR_WHEEL_POS(player)[wheel & 2][2];
    orth[1] += m->wheelHeight[wheel | 1] - m->wheelHeight[wheel & 2];
    vector_direction(orth, dir);
    for (i = 0; i < 3; i++) {
        wid = dir[i] * 0.75f;
        slot->pos[i] = PC_CAR_WHEEL_POS(player)[wheel][i];
        slot->left[i] = slot->pos[i] - wid;
        slot->right[i] = slot->pos[i] + wid;
    }
    up = skid_lift;
    slot->left[1] += up;
    slot->right[1] += up;
    obj->f8 = *(f32 *) ((uintptr_t) &gSeconds[0]);
    obj->color[0] = color[0];
    obj->color[1] = color[1];
    obj->color[2] = color[2];
    obj->color[3] = 0xC0;
    ZOID_UpdatePoly(obj->dl, 4, slot->quad, 0, obj->color, 0, 0);
}

/* The manifest owners of GetSkid and ContinueSkid are empty deleted-procedure stubs
 * (src/blob/GetSkid.c, ContinueSkid.c), which this group's calls bind to on PC. The bodies
 * written here get their own names (docs/port/PROTOTYPES.md). */
#define GetSkid pc_GetSkid
#define ContinueSkid pc_ContinueSkid
/* GetSkid = arcade GetSkid: take a skid from the pool, or steal the one farthest from the camera.
 * Not `static`, not in `keep`: umerge inlines it into its only caller and leaves the retail `jr ra` stub. */
EffectObj *GetSkid(u8 *color)
{
    s32 i;
    EffectObj *s;
    EffectObj *fars;
    f32 d;
    f32 dsq;
    f32 fardsq;

    s = GetStatLLE(gSkidList);
    if (s == 0) {
        fardsq = -1.0f;
        fars = ((EffectObj **) gSkidList)[POOL_HEAD];
        for (s = ((EffectObj **) gSkidList)[POOL_HEAD]; s->next; s = s->next) {
            for (dsq = 0.0f, i = 0; i < 3; i++) {
                d = s->pos[i] - gCamera.pos[i];
                dsq += d * d;
            }
            if (dsq > fardsq) {
                fardsq = dsq;
                fars = s;
            }
        }
        s = fars;
        ZOID_RemovePoly(s->dl);
        FreeStatLLE(gSkidList, s);
        s = GetStatLLE(gSkidList);
    }
    s->f20 = 0;
    s->color[0] = color[0];
    s->color[1] = color[1];
    s->color[2] = color[2];
    s->color[3] = 0xC0;
    s->dl = ZOID_NewPoly(4, gDummyVtx, gTexList[8], color,
                          ((gGameState & 0x100) ? 1 : 15) | 0x1200, 1);
    if (s->dl == 0) {
        FreeStatLLE(gSkidList, s);
        return 0;
    }
    return s;
}

/* StartSkid (0x800AFB38, 135 words) = arcade StartSkid.  IPA parameters: ns $s6, slot $s3, tire $s4,
 * color $s5.  The three veccopy() macros are written out element by element as in vecmath.h. */
void StartSkid(WheelSlot *ns, s16 slot, u32 tire, u8 *color)
{
    EffectObj *s;

    s = GetSkid(color);
    if (s == 0) {
        return;
    }
    ns->obj = s;
    ns->lenSq = 0.0f;
    UpdateSkid(ns, slot, tire, color);
    ns->start[0] = ns->pos[0]; ns->start[1] = ns->pos[1]; ns->start[2] = ns->pos[2];
    ns->quad[0] = ns->right[0]; ns->quad[1] = ns->right[1]; ns->quad[2] = ns->right[2];
    ns->quad[3] = ns->left[0]; ns->quad[4] = ns->left[1]; ns->quad[5] = ns->left[2];
    ZOID_UpdatePoly(s->dl, 4, ns->quad, 0, 0, 0, 0);
}

#define magsq(v) (((v)[0] * (v)[0]) + ((v)[1] * (v)[1]) + ((v)[2] * (v)[2]))

/* ContinueSkid = arcade ContinueSkid: extend the current skid, or end it and start a new one when the
 * tire has left the skid's line by more than 1.0 (squared) or the skid got shorter.  Inlined into UpdateSkids.
 * No named `car` here and magsq as a macro: with either as a named local / inline function the frame is one
 * word too big.  0.1f is UpdateSkids's own .rodata (0x80123C0C, hoisted into $f26): natural literal,
 * reported unverified by the scorer (an extern is not hoisted out of the loop). */
void ContinueSkid(WheelSlot *ns, s16 slot, u32 tire, u8 *color)
{
    f32 vec[3];
    f32 dirpos[3];
    f32 lensq;
    f32 len;
    f32 ds;
    f32 invlen;

    UpdateSkid(ns, slot, tire, color);

    vec[0] = PC_CAR_WHEEL_POS(slot)[tire][0] - ns->start[0];
    vec[1] = PC_CAR_WHEEL_POS(slot)[tire][1] - ns->start[1];
    vec[2] = PC_CAR_WHEEL_POS(slot)[tire][2] - ns->start[2];
    lensq = magsq(vec);

    if (ns->lenSq > 0.0f) {
        len = sqrtf(lensq);
        dirpos[0] = ns->dir[0] * len;
        dirpos[1] = ns->dir[1] * len;
        dirpos[2] = ns->dir[2] * len;
        vec[0] = ns->pos[0] - dirpos[0];
        vec[1] = ns->pos[1] - dirpos[1];
        vec[2] = ns->pos[2] - dirpos[2];
        ds = (vec[0] * vec[0]) + (vec[1] * vec[1]) + (vec[2] * vec[2]);
        if ((ds > 1.0f) || (lensq < ns->lenSq)) {
            StopSkid(ns);
            StartSkid(ns, slot, tire, color);
        }
    } else if (lensq > 0.1f) {
        invlen = 1.0f / sqrtf(lensq);
        ns->dir[0] = vec[0] * invlen;
        ns->dir[1] = vec[1] * invlen;
        ns->dir[2] = vec[2] * invlen;
        ns->lenSq = lensq;
    }
}

/* UpdateSkids (0x800AFD5C, 265 words): for each tire, start / continue / stop its skid (arcade DoSkid).
 * `slot` is int here (no entry sll/sra; the s16 callee parameters are read back with `lh` from its home).
 * Quirks: colour chosen with ?: (the if-form drops the `b` and swaps $t0/$t2); `flags & mask[i]` operand
 * order; `ns` walks with the loop. */
void UpdateSkids(s32 slot)
{
    s32 i;
    CarState *m;
    WheelSlot *ns;
    s32 on;
    s32 laston;
    u8 *color;

    if (gNumPlayers >= 3) {
        return;
    }
    m = PC_MODEL(slot);
    ns = gNewSkid[slot];
    for (i = 0; i < 4; i++, ns++) {
        on = ((PC_CAR_FLAGS(slot) & skid_tire_flags[i]) != 0) && (m->airborne[0] == -1) &&
             (m->terrain[i] != 8) && ((stree_surf[m->surface[i]].flags & 0x30) == 0);
        laston = (ns->obj != 0);
        color = (m->terrain[i] == 0) ? gShadowColor : gDirtColor;

        if (!laston && on) {
            StartSkid(ns, slot, i, color);
        } else if (laston && on) {
            ContinueSkid(ns, slot, i, color);
        } else if (laston && !on) {
            StopSkid(ns);
        }
    }
}

