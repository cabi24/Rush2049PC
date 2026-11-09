#include "game/game.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;
typedef union Color4 {struct {u8 r,g,b,a;} c;u32 rgba;} Color4;
typedef struct Effect {
    struct Effect *next;
    s16 view;       /* 4 */
    s16 mesh;       /* 6 */
    s16 player;     /* 8 */
/* PC: a visual (tVisual) is 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
    u8 pad10[2];
    u32 data;
    f32 time;
    void (*callback)(struct Effect *, s16);
} Effect;
typedef struct CAR_DATA {
    u8 pad0[8];
    f32 pos[3];             /* 8 */
    u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad14_308[0x358 - 0x308];
    s8 hidden;              /* 856 */
    u8 pad359[0x380 - 0x359]; void *input; u8 pad359_384[0x38C - 0x384];
    u32 flags;              /* 908 */
    u8 pad390[928 - 912];
    s8 color_index;         /* 928 */
    u8 alpha;               /* 929 */
    s8 alpha_mode;          /* 930 */
    u8 pad3A3[952 - 931];
} CAR_DATA;
typedef struct MODELDAT {
    u8 pad0[1732];
    s16 state;              /* 1732 */
    u8 pad6C6[1990 - 1734];
    s16 player;             /* 1990 */
    u8 pad7C8[1996 - 1992];
    s8 mode;                /* 1996 */
    u8 pad7CD[2056 - 1997];
} MODELDAT;
typedef struct Camera {
    f32 mat[3][3];             /* 0 */
    Vec3 pos;               /* 36 */
    f32 basis[3][3];           /* 48 */
    u8 pad54[152 - 84];
} Camera;
typedef struct View {
    void *first; void *bounds; u8 pad0_8[0xC - 0x8];
    f32 radius;             /* 12 */
    f32 scale;              /* 16 */
    u8 pad14[72 - 20];
} View;
typedef struct Vertex20 {f32 x,y,z;s16 s,t;u32 color;} Vertex20;
typedef struct Mesh {s16 count;u16 flags,parameter,unknown;Vertex20 vertices[4];} Mesh;

VIEW(Camera[], gCamera);
#define gCamera gCamera_view
VIEW(View[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view
VIEW(Mesh[], PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view
VIEW(s8[4][4], gFadePlayerArrow);
#define gFadePlayerArrow gFadePlayerArrow_view
VIEW(s32, gGameState);
#define gGameState gGameState_view
VIEW(Color4[5], arrowColors);
#define arrowColors arrowColors_view
VIEW(Color4, arrow_base_color);
#define arrow_base_color arrow_base_color_view

f32 gDummyVtx[4][3] = { { 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
extern void ZOID_UpdatePoly(Mesh *, s32, f32 (*)[3], u16, Color4 *, u16, s32);

void AnimateArrow(Effect *effect, s16 op) {
    CAR_DATA *record;
    MODELDAT *vehicle;
    f32 quad[4][3];
    f32 angle;
    s32 type;
    s32 i;
    Color4 color;
    f32 delta[3];
    View *view;
    f32 x, y, depth, len, slope, threshold;
    u32 side;

    record = PC_CAR(effect->player);
    vehicle = PC_MODEL(effect->player);
    type = effect->view;
    color = arrow_base_color;
    if (!gShowPlayerArrows || !(gGameState & 0x400000) || (!(!record->hidden && vehicle->state < 0) && gRace == 6)) {
        ZOID_HidePoly(&PolyInstanceList[effect->mesh]);
        return;
    }
    if (gRace == 6 && (record->flags & 1)) {
        if (record->alpha_mode == 1) {
            color.c.a = record->alpha;
        } else if (record->alpha_mode == 2) {
            color.c.a = record->alpha;
        } else {
            color.c.a = arrowinvsColor[record->color_index];
        }
        if (color.c.a < 32) {
            color.c.a = 32;
        }
    }
    if (vehicle->mode == 2) {
        if (gRace == 6) {
            color.c.r = arrowColors[gCarTeam[vehicle->player]].c.r;
            color.c.g = arrowColors[gCarTeam[vehicle->player]].c.g;
            color.c.b = arrowColors[gCarTeam[vehicle->player]].c.b;
        } else {
            color.c.r = arrowColors[vehicle->player].c.r;
            color.c.g = arrowColors[vehicle->player].c.g;
            color.c.b = arrowColors[vehicle->player].c.b;
        }
        if (gFadePlayerArrow[vehicle->player][type] && color.c.a > 112) {
            color.c.a = 112;
        }
        if (gRace == 2 && effect->player > 0) {
            color.c.a = 128;
        }
    } else {
        color.c.r = arrowColors[4].c.r;
        color.c.g = arrowColors[4].c.g;
        color.c.b = arrowColors[4].c.b;
    }
    delta[0] = record->pos[0] - gCamera[type].pos.x;
    delta[1] = record->pos[1] - gCamera[type].pos.y;
    delta[2] = record->pos[2] - gCamera[type].pos.z;
    if (gRace == 6) {
        view = &ZoidWinInfo[type];
        U64WorldVector(&delta[0], &quad[0][0], gCamera[type].mat);
        angle = fatan2(-quad[0][0], quad[0][2]);
        len = 19.0f;
        depth = 31.415927f / view->radius;
        if (gSplitScreen >= 2) {
            slope = 16.5f;
            threshold = view->scale * 0.75f;
        } else {
            slope = 14.5f;
            threshold = view->scale * 1.1f;
        }
        if (angle < -view->radius * 0.48f && angle > -1.5707964f) {
            x = view->radius * 0.63661975f * len;
            angle = -angle;
            side = 3;
            goto tail;
        }
        if (angle < -1.5707964f) {
            angle = angle + 3.1415927f;
            goto behind;
        }
        if (1.5707964f < angle) {
            angle = angle - 3.1415927f;
behind:
            x = view->radius * 0.63661975f * len * angle * 0.63661975f;
            y = view->scale * 0.63661975f * slope;
            side = 0;
            goto draw;
        }
        if (view->radius * 0.48f < angle && angle < 1.5707964f) {
            x = view->radius * 0.63661975f * -len;
            side = 1;
tail:
            y = view->scale * 0.63661975f * slope * (angle - threshold) * (1.0f / (1.5707964f - threshold));
draw:
            for (i = 0; i < 4; i++) {
                quad[side][0] = gDummyVtx[i][0] + x;
                quad[side][1] = gDummyVtx[i][1] - y;
                quad[side][2] = depth;
                side = (side + 1) & 3;
        }
            ZOID_UpdatePoly(&PolyInstanceList[effect->mesh], 4, quad, 0, &color, (1 << type) | 0x3610, 0);
            return;
        }
    }

    angle = vector_magnitude(&delta[0]) / 25.0f;
    for (i = 0; i < 4; i++) {
        U64WorldVector(&gDummyVtx[i][0], &quad[i][0], gCamera[type].basis);
        quad[i][0] = quad[i][0] * angle + record->pos[0];
        quad[i][1] = quad[i][1] * angle + record->pos[1] + angle + 5.0f;
        quad[i][2] = quad[i][2] * angle + record->pos[2];
    }
    if (gRace == 6) {
        ZOID_UpdatePoly(&PolyInstanceList[effect->mesh], 4, quad, 0, &color, (1 << type) | 0x3600, 0);
    } else {
        ZOID_UpdatePoly(&PolyInstanceList[effect->mesh], 4, quad, 0, &color, (1 << type) | 0x1600, 0);
    }
}
