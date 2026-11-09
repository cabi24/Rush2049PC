#include "game/game.h"

typedef struct Blit {
    /* 0x00 */ char *Name;
    /* 0x04 */ void *Image;
    /* 0x08 */ void *Info;
    /* 0x0C */ s16 TexIndex;
    /* 0x0E */ s16 X;
    /* 0x10 */ s16 Y;
    /* 0x12 */ u16 State;
    /* 0x14 */ s16 Width;
    /* 0x16 */ s16 Height;
    /* 0x18 */ u8 Alpha, Flip;
    /* 0x1A */ s8 Hide;
    /* 0x1B */ u8 Init;
    /* 0x1C */ s16 Top, Bot, Left, Right;
    /* 0x24 */ s32 pad24;
    /* 0x28 */ void *AnimFunc;
    /* 0x2C */ u32 AnimID;
} Blit;

typedef struct MODELDAT {
    u8 pad0[1990];
    s16 index;              /* 1990 */
    s16 in_game;            /* 1992 */
    u8 pad7CA[2];
    s8 drone_type;          /* 1996 */
    u8 pad7CD[2056 - 1997];
} MODELDAT;
typedef struct CAR_DATA {
    u8 pad0[8];
    f32 dr_pos[3];          /* 8 */
    u8 pad14[44 - 20];
    f32 dr_uvs[3][3];       /* 44 */
    u8 pad50[0xEF - 80];
    s8 state;
    u8 padF0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 padF0_308[0x380 - 0x308]; void *input; u8 padF0_384[0x3B8 - 0x384];
} CAR_DATA;
typedef struct { s32 x, y; } Pos;

VIEW(s32, gGameState);
#define gGameState gGameState_view
VIEW(Pos[][4], radarPos);
#define radarPos radarPos_view

s32 animRadarBlip(Blit *blt) {
    s32 me;
    MODELDAT *m;
    CAR_DATA *gc;
    CAR_DATA *mc;
    f32 scale;
    s16 slot;
    f32 rpos[3];
    f32 lpos[3];
    f32 ldir[3];
    s16 angle, flip;
    s32 hw;
    s32 hh;

    me = (blt->AnimID >> 8) & 0xF;
    hw = blt->Width * 0.125f;
    hh = blt->Height;
    if (me >= gNumWindows || (gGameState & 8) || gNumCars < 2) {
        blt->AnimFunc = 0;
        return Hidden(blt, 1);
    }
    slot = blt->AnimID & 0xF;
    if (Hidden(blt, !gShowRadar || PC_CAR_FINISHED(me) == 1)) {
        return 1;
    }
    m = PC_MODEL(slot);
    gc = PC_CAR(slot);
    if (!m->in_game) {
        blt->AnimFunc = 0;
        return Hidden(blt, 1);
    }
    if (Hidden(blt, slot != me && !AreWeSolid(slot))) {
        return 1;
    }
    mc = PC_CAR(me);
    lpos[0] = gc->dr_pos[0] - mc->dr_pos[0];
    lpos[1] = gc->dr_pos[1] - mc->dr_pos[1];
    lpos[2] = gc->dr_pos[2] - mc->dr_pos[2];
    BodyVector(lpos, rpos, mc->dr_uvs[0]);
    blt->X = radarWidth * 0.5f;
    blt->Y = 16;
    if (gMirrorWaves) {
        scale = (1.0f / 480.0f);
        blt->X += -rpos[0] * scale * radarWidth;
    } else {
        scale = (1.0f / 480.0f);
        blt->X += rpos[0] * scale * radarWidth;
    }
    blt->Y += rpos[2] * -scale * radarHeight;
    if (Hidden(blt, blt->X < hw || radarWidth - hw < blt->X || blt->Y < hh || radarHeight - hh < blt->Y)) {
        return 1;
    }
    blt->X += radarPos[gNumWindows - 1][me].x;
    blt->Y += radarPos[gNumWindows - 1][me].y;
    if (m->drone_type == 2) {
        SelectBlit(blt, m->index, blt->Width * 0.125f, blt->Height);
    } else if (m->drone_type == 1) {
        SelectBlit(blt, 5, blt->Width * 0.125f, blt->Height);
    }
    blt->X -= hw / 2;
    blt->Y -= hh / 2;
    UpdateBlit(blt);
    return 1;
}
