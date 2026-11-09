/* Skid marks (MAT3 skids.c, the arcade's): while a tyre skids it lays a quad behind it on the road, from where the
 * skid started to where the tyre is; a skid that turns or shortens ends and a new one starts. The quads come from a
 * pool (gSkidList); when it is empty the skid farthest from the camera is taken. */
#include "game/game.h"

float sqrtf(float);
#pragma intrinsic (sqrtf)

/* A skid's quad (gSkidList's element), the PC's layout. */
typedef struct EffectObj {
    struct EffectObj *next;     /* the pool's links (GetStatLLE) */
    void *prev;
    f32 f8;                     /* when it was last extended (gSeconds) */
    void *dl;                   /* its polygon (ZOID_NewPoly) */
    f32 pos[3];
    u8 color[4];
    s32 f20;
} EffectObj;

/* A tyre's skid (pc_skids[car][tyre]). */
typedef struct WheelSlot {
    EffectObj *obj;             /* the skid's quad, or 0 when the tyre isn't skidding */
    f32 start[3];               /* where the skid started */
    f32 pos[3];                 /* where the tyre is */
    f32 lenSq;                  /* the skid's length squared, once it has a direction */
    f32 dir[3];                 /* its direction */
    f32 quad[6];                /* the quad's start edge (right, left), then its moving edge: */
    f32 left[3];
    f32 right[3];
} WheelSlot;

/* The car record's fields the skids read (MAT3 tModelData). */
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

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the skid state is pc_skids, PC_MAX_LINKS cars of four tires, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6), with WheelSlot's field names. */
#define gNewSkid pc_skids

VIEW(u32[4], skid_tire_flags);
#define skid_tire_flags skid_tire_flags_view

extern f32 gDummyVtx[];

VIEW(f32[], gSeconds);
#define gSeconds gSeconds_view

extern void ZOID_RemovePoly(void *dl);
extern void *ZOID_NewPoly(s32, f32 *, u16, u8 *, s32, s32);
extern void ZOID_UpdatePoly(void *dl, s32 n, f32 *quad, s32, u8 *color, s32, s32);

/* Moves skid slot's leading edge to tyre wheel of player's car: across the axle (the direction from tyre wheel & 2
 * to tyre wheel | 1, with their heights), 0.75 to either side of the tyre and lifted by skid_lift; stamps the time
 * and the colour, and updates the quad. */
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
/* A quad for a new skid of colour color: from the pool, or the skid farthest from the camera taken back; its
 * polygon made (in the front end's car screen with another draw mode). 0 when no polygon could be made. */
EffectObj *GetSkid(u8 *color)
{
    s32 i;
    EffectObj *s;
    EffectObj *fars;
    f32 d;
    f32 dsq;
    f32 fardsq;

    s = GetStatLLE(&gSkidList);
    if (s == 0) {
        fardsq = -1.0f;
        fars = (EffectObj *) gSkidList.head;
        for (s = (EffectObj *) gSkidList.head; s->next; s = s->next) {
            for (dsq = 0.0f, i = 0; i < 3; i++) {
                d = s->pos[i] - gCamera[0].mat.pos[i];
                dsq += d * d;
            }
            if (dsq > fardsq) {
                fardsq = dsq;
                fars = s;
            }
        }
        s = fars;
        ZOID_RemovePoly(s->dl);
        FreeStatLLE(&gSkidList, s);
        s = GetStatLLE(&gSkidList);
    }
    s->f20 = 0;
    s->color[0] = color[0];
    s->color[1] = color[1];
    s->color[2] = color[2];
    s->color[3] = 0xC0;
    s->dl = ZOID_NewPoly(4, gDummyVtx, gTexList[8], color,
                          ((gGameState & GAME_STATE_CAR) ? 1 : 15) | 0x1200, 1);
    if (s->dl == 0) {
        FreeStatLLE(&gSkidList, s);
        return 0;
    }
    return s;
}

/* Starts a skid for tyre tire of car slot (ns its skid): a new quad, both edges at the tyre. Nothing when no quad
 * is free. */
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

/* Extends tyre tire's skid to the tyre; once it is longer than 0.1 (squared) it has a direction, and from then
 * on the skid ends and a new one starts when the tyre leaves its line by more than 1 (squared) or the skid gets
 * shorter. */
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

/* Each frame, each tyre of car slot (one or two players only): it skids when the car's skid flag for it is set,
 * the car is on the ground, the tyre isn't on terrain 8 and its surface allows marks (flags 0x30 clear); a skid
 * starts, goes on or stops accordingly, dark on road (terrain 0, gShadowColor), dirt-coloured elsewhere (arcade
 * DoSkid). */
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
             (m->terrain[i] != 8) && ((stree_surf[m->surface[i]].snd_flags & 0x30) == 0);
        laston = (ns->obj != 0);
        color = (m->terrain[i] == 0) ? gShadowColor.c : gDirtColor;

        if (!laston && on) {
            StartSkid(ns, slot, i, color);
        } else if (laston && on) {
            ContinueSkid(ns, slot, i, color);
        } else if (laston && !on) {
            StopSkid(ns);
        }
    }
}

