/* animate_arrow/group.c: the arrows over the human players' cars in a split-screen race (MAT3 visuals.c,
 * AnimateArrow(); and gDummyVtx).
 *
 * StartArrow gives each human's car one arrow per view (a polygon, a car visual whose index is the view): the
 * other players see where that car is. In a battle the arrow of a car off the edge of a view, or behind it, is
 * drawn on the view's edge instead, pointing the way to turn. */
#include "game/game.h"

typedef struct Vec3 { f32 x, y, z; } Vec3;
typedef union tRGBA {struct {u8 r,g,b,a;} c;u32 rgba;} tRGBA;
/* A car visual (MAT3 tVisual). PC: 0x20 bytes: next and func are host pointers, data a u32 (docs/port/CARS.md). */
typedef struct tVisual {
    struct tVisual *next;
    s16 index;      /* 4: the view this arrow is drawn in */
    s16 objnum;     /* 6: its polygon (PolyInstanceList) */
    s16 slot;       /* 8: the car it marks */
    u8 pad10[2];
    u32 data;
    f32 timeStamp;
    void (*func)(struct tVisual *, s16);
} tVisual;
/* The car record (tCarData, N64 offsets; docs/port/CARS.md 2): its place, its crash and its battle state. */
typedef struct tCarData {
    u8 pad0[8];
    f32 dr_pos[3];          /* 8 */
    u8 pad14[0x110 - 0x14]; struct pc_visual visuals[21]; u8 pad308[0x358 - 0x308];
    s8 crash_state;         /* 856 */
    u8 pad359[0x380 - 0x359]; void *player; u8 pad384[0x38C - 0x384];
    u32 defense_active;     /* 908: battle.defense.active (bit 0: invisible) */
    u8 pad390[928 - 912];
    s8 shoot;               /* 928: battle.shoot */
    u8 fadeAlpha;           /* 929: battle.fadeAlpha */
    s8 fadeState;           /* 930: battle.fadeState */
    u8 pad3A3[952 - 931];
} tCarData;
/* The model record (tModelData): the respawn's state, the slot, who drives. */
typedef struct tModelData {
    u8 pad0[1732];
    s16 resurrect_moving_state; /* 1732: RES_* */
    u8 pad6C6[1990 - 1734];
    s16 slot;               /* 1990 */
    u8 pad7C8[1996 - 1992];
    s8 drone_type;          /* 1996: 1 a drone, 2 a human (MAT3 DRONE_TYPE_AI, DRONE_TYPE_HUMAN) */
    u8 pad7CD[2056 - 1997];
} tModelData;
/* A view's window (ZoidWinInfo[view], MAT3 tWindowInfo): its view angles. PC: the two pointers are host ones. */
typedef struct tWindowInfo {
    void *vp; void *s; u8 pad8[0xC - 0x8];
    f32 whviewang;          /* 12: the horizontal view angle, radians */
    f32 wvviewang;          /* 16: the vertical one */
    u8 pad14[72 - 20];
} tWindowInfo;
/* A scene polygon (PolyInstanceList, MAT3 tPoly): four vertices. */
typedef struct Vertex20 {f32 x,y,z;s16 s,t;u32 color;} Vertex20;
typedef struct tPoly {s16 count;u16 flags,parameter,unknown;Vertex20 verts[4];} tPoly;

VIEW(tWindowInfo[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view
VIEW(tPoly[], PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view
VIEW(s8[4][4], gFadePlayerArrow);
#define gFadePlayerArrow gFadePlayerArrow_view
VIEW(s32, gGameState);
#define gGameState gGameState_view
VIEW(tRGBA[5], arrowColors);
#define arrowColors arrowColors_view
VIEW(tRGBA, arrow_base_color);
#define arrow_base_color arrow_base_color_view

/* A unit square in the xy plane: the shape of the arrows, skid marks and other one-polygon effects. */
f32 gDummyVtx[4][3] = { { 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
extern void ZOID_UpdatePoly(tPoly *, s32, f32 (*)[3], u16, tRGBA *, u16, s32);

/* The arrow v draws for car v->slot in view v->index, once a frame (`op` is not used). Hidden but while racing
 * (gGameState 0x400000: GS_RACING) with player arrows on, and in a battle while the car is wrecked or respawning.
 * Its colour: the player's (the team's in a battle), for drones the fifth; translucent while the car is invisible in
 * a battle (its fade, at least 32), at most 112 alpha where gFadePlayerArrow asks, half in a ghost race but for the
 * first car. In a battle, a car outside 0.48 of the view's horizontal angle to either side, or behind, gets a screen
 * arrow on that edge, raised by how far round it is (the constants are 2/pi, pi/2 and pi; 31.415927 / whviewang is
 * the depth that makes the square 19 units wide). Otherwise the arrow floats 5 ft (plus its size) over the car,
 * facing the camera, its size a 25th of the distance so it looks the same at any range. */
void AnimateArrow(tVisual *v, s16 op) {
    tCarData *car;
    tModelData *m;
    f32 xyz[4][3];
    f32 scale;
    s32 oslot;
    s32 i;
    tRGBA clr;
    f32 vec[3];
    tWindowInfo *window;
    f32 x, y, z, border, vscale, voffset;
    u32 j;

    car = PC_CAR(v->slot);
    m = PC_MODEL(v->slot);
    oslot = v->index;
    clr = arrow_base_color;
    if (!gShowPlayerArrows || !(gGameState & 0x400000) || (!(!car->crash_state && m->resurrect_moving_state < 0) && gRace == 6)) {
        ZOID_HidePoly(&PolyInstanceList[v->objnum]);
        return;
    }
    if (gRace == 6 && (car->defense_active & 1)) {     /* kBattle */
        if (car->fadeState == 1) {
            clr.c.a = car->fadeAlpha;
        } else if (car->fadeState == 2) {
            clr.c.a = car->fadeAlpha;
        } else {
            clr.c.a = arrowinvsColor[car->shoot];
        }
        if (clr.c.a < 32) {
            clr.c.a = 32;
        }
    }
    if (m->drone_type == 2) {                         /* DRONE_TYPE_HUMAN */
        if (gRace == 6) {
            clr.c.r = arrowColors[gCarTeam[m->slot]].c.r;
            clr.c.g = arrowColors[gCarTeam[m->slot]].c.g;
            clr.c.b = arrowColors[gCarTeam[m->slot]].c.b;
        } else {
            clr.c.r = arrowColors[m->slot].c.r;
            clr.c.g = arrowColors[m->slot].c.g;
            clr.c.b = arrowColors[m->slot].c.b;
        }
        if (gFadePlayerArrow[m->slot][oslot] && clr.c.a > 112) {
            clr.c.a = 112;
        }
        if (gRace == 2 && v->slot > 0) {                /* kGhost */
            clr.c.a = 128;
        }
    } else {
        clr.c.r = arrowColors[4].c.r;
        clr.c.g = arrowColors[4].c.g;
        clr.c.b = arrowColors[4].c.b;
    }
    vec[0] = car->dr_pos[0] - gCamera[oslot].mat.pos[0];
    vec[1] = car->dr_pos[1] - gCamera[oslot].mat.pos[1];
    vec[2] = car->dr_pos[2] - gCamera[oslot].mat.pos[2];
    if (gRace == 6) {
        /* the car's bearing from the camera: 0 straight ahead, negative to the -x side */
        window = &ZoidWinInfo[oslot];
        U64WorldVector(&vec[0], &xyz[0][0], gCamera[oslot].mat.uvs[0]);
        scale = fatan2(-xyz[0][0], xyz[0][2]);
        border = 19.0f;
        z = 31.415927f / window->whviewang;
        if (gSplitScreen >= 2) {
            vscale = 16.5f;
            voffset = window->wvviewang * 0.75f;
        } else {
            vscale = 14.5f;
            voffset = window->wvviewang * 1.1f;
        }
        if (scale < -window->whviewang * 0.48f && scale > -1.5707964f) {
            x = window->whviewang * 0.63661975f * border;
            scale = -scale;
            j = 3;
            goto side_arrow;
        }
        if (scale < -1.5707964f) {
            scale = scale + 3.1415927f;
            goto behind;
        }
        if (1.5707964f < scale) {
            scale = scale - 3.1415927f;
behind:
            /* behind the camera: on the bottom edge, as far along as the car is round */
            x = window->whviewang * 0.63661975f * border * scale * 0.63661975f;
            y = window->wvviewang * 0.63661975f * vscale;
            j = 0;
            goto draw;
        }
        if (window->whviewang * 0.48f < scale && scale < 1.5707964f) {
            x = window->whviewang * 0.63661975f * -border;
            j = 1;
side_arrow:
            /* off a side: on that edge, the higher the further round */
            y = window->wvviewang * 0.63661975f * vscale * (scale - voffset) * (1.0f / (1.5707964f - voffset));
draw:
            /* the square at (x, -y), its corners turned to point j's way */
            for (i = 0; i < 4; i++) {
                xyz[j][0] = gDummyVtx[i][0] + x;
                xyz[j][1] = gDummyVtx[i][1] - y;
                xyz[j][2] = z;
                j = (j + 1) & 3;
            }
            ZOID_UpdatePoly(&PolyInstanceList[v->objnum], 4, xyz, 0, &clr, (1 << oslot) | 0x3610, 0);
            return;
        }
    }

    scale = vector_magnitude(&vec[0]) / 25.0f;
    for (i = 0; i < 4; i++) {
        U64WorldVector(&gDummyVtx[i][0], &xyz[i][0], gCamera[oslot].n64.uvs[0]);
        xyz[i][0] = xyz[i][0] * scale + car->dr_pos[0];
        xyz[i][1] = xyz[i][1] * scale + car->dr_pos[1] + scale + 5.0f;
        xyz[i][2] = xyz[i][2] * scale + car->dr_pos[2];
    }
    if (gRace == 6) {
        ZOID_UpdatePoly(&PolyInstanceList[v->objnum], 4, xyz, 0, &clr, (1 << oslot) | 0x3600, 0);
    } else {
        ZOID_UpdatePoly(&PolyInstanceList[v->objnum], 4, xyz, 0, &clr, (1 << oslot) | 0x1600, 0);
    }
}
