/* animRadarBlip: a car's blip on a player's radar, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

#define RADAR_RANGE 480.0f       /* the radar's reach across, feet */

/* A model record (MAT3 tModelData, 0x808 bytes) as this reads it. */
typedef struct MODELDAT {
    u8 pad0[1990];
    s16 index;              /* 1990 */
    s16 in_game;            /* 1992 */
    u8 pad7CA[2];
    s8 drone_type;          /* 1996 */
    u8 pad7CD[2056 - 1997];
} MODELDAT;
/* A car record as this reads it: its position and orientation. */
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

/* The blip of car animID & 0xF on the radar of player (animID >> 8) & 0xF: the car's position relative to the
 * player's car, turned into the player's car's frame, scaled to the radar (RADAR_RANGE feet across; mirrored with
 * gMirrorWaves), and drawn as the car's own frame of the blip strip (a human's: its index; a drone: frame 5). Hidden
 * off the radar, when the radar or the car is not shown, or the car is not solid; its animation stops past the
 * windows, in attract mode, with one car, or for a car out of the game. Returns 1 (or Hidden's answer). */
s32 animRadarBlip(tBlit *blt) {
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

    me = (blt->animID >> 8) & 0xF;
    hw = blt->width * 0.125f;
    hh = blt->height;
    if (me >= gNumWindows || (gGameState & GS_DEMO) || gNumCars < 2) {
        blt->animFunc = 0;
        return Hidden(blt, 1);
    }
    slot = blt->animID & 0xF;
    if (Hidden(blt, !gShowRadar || PC_CAR_FINISHED(me) == 1)) {
        return 1;
    }
    m = PC_MODEL(slot);
    gc = PC_CAR(slot);
    if (!m->in_game) {
        blt->animFunc = 0;
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
    blt->x = radarWidth * 0.5f;
    blt->y = 16;
    if (gMirrorWaves) {
        scale = (1.0f / RADAR_RANGE);
        blt->x += -rpos[0] * scale * radarWidth;
    } else {
        scale = (1.0f / RADAR_RANGE);
        blt->x += rpos[0] * scale * radarWidth;
    }
    blt->y += rpos[2] * -scale * radarHeight;
    if (Hidden(blt, blt->x < hw || radarWidth - hw < blt->x || blt->y < hh || radarHeight - hh < blt->y)) {
        return 1;
    }
    blt->x += radarPos[gNumWindows - 1][me].x;
    blt->y += radarPos[gNumWindows - 1][me].y;
    if (m->drone_type == 2) {
        SelectBlit(blt, m->index, blt->width * 0.125f, blt->height);
    } else if (m->drone_type == 1) {
        SelectBlit(blt, 5, blt->width * 0.125f, blt->height);
    }
    blt->x -= hw / 2;
    blt->y -= hh / 2;
    UpdateBlit(blt);
    return 1;
}
