#include "game/game.h"

typedef struct Slot {
    /* 0x00 */ s8 busy;
    /* 0x01 */ s8 enabled;
    /* 0x02 */ s8 unk2;
    /* 0x03 */ s8 player;
    /* 0x04 */ u8 primary[4];
    /* 0x08 */ u8 secondary[4];
    /* 0x0C */ u8 previous[4];
    /* 0x10 */ s8 present;
    /* 0x14 */ s32 mode;
    /* 0x18 */ s16 arg[4];
    /* 0x20 */ u8 pad20[8];
    /* 0x28 */ u32 deadline;
} Slot; /* 0x2C */

typedef struct {
    u8 pad0[6];
    s8 present;
    u8 pad7[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} Controller;

typedef struct {
    u8 pad0[0x27C];
    volatile u32 unk27C;
} Sched;

VIEW(Slot[4][2], gPakError);
#define gPakError gPakError_view
VIEW(Controller[], gPaks);
#define gPaks gPaks_view

VIEW(Sched, gScheduler);
#define gScheduler gScheduler_view

void SetPakError(s32 player, s32 mode, s32 arg2, s32 arg3, s32 arg4, s32 arg5) {
    s32 i;
    s32 j;
    s8 present;
    static s8 sInited = 0;

    if (mode == 10) {
        pak_recognize_error_seen = 1;
    }
    if (!sInited) {
        for (j = 0; j < 4; j++) {
            for (i = 0; i < 2; i++) {
                gPakError[j][i].mode = 0;
                gPakError[j][i].busy = 1;
            }
        }
        sInited = 1;
    }
    for (j = 0; j < 2; j++) {
        if (gPakError[player][j].mode == 0) {
            break;
        }
    }
    /* Original game: when both of the pak's records are in use the loop ends with j == 2 and the error is written
     * into the record past the row: the next pak's first one, or past gPakError for pak 3. MAT3 pakerr.c has the
     * same loop over three records. Kept as retail. */
    gPakError[player][j].busy = 0;
    gPakError[player][j].deadline = (s32)(3.0f * gRetraceRate) + gScheduler.unk27C;
    gPakError[player][j].enabled = 1;
    gPakError[player][j].unk2 = 0;
    gPakError[player][j].mode = mode;
    for (i = 0; i < 4; i++) {
        gPakError[player][j].primary[i] = GetControllerActive(i);
        gPakError[player][j].secondary[i] = GetControllerStickEdges(i);
        gPakError[player][j].previous[i] = 0;
    }
    SetControllerActive(-1, 0);
    SetControllerStickEdges(-1, 0);
    if (mode == 1) {
        SetControllerActive(-1, 1);
        SetControllerStickEdges(-1, 1);
    } else {
        gPakError[player][j].player = player;
        if (mode == 2) {
            SetControllerActive(-1, 1);
            SetControllerStickEdges(-1, 1);
        } else {
            SetControllerActive(gPakError[player][j].player, 1);
            SetControllerStickEdges(gPakError[player][j].player, 1);
        }
    }
    present = gPaks[player].present;
    gPakError[player][j].arg[0] = arg2;
    gPakError[player][j].present = present;
    gPakError[player][j].arg[1] = arg3;
    gPakError[player][j].arg[2] = arg4;
    gPakError[player][j].arg[3] = arg5;
}
