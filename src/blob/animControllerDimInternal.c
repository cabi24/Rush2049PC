/* animControllerDimInternal: the "no Rumble Pak" dimming of the rumble screen (MAT3 pausescr.c). */
#include "game/game.h"

/* A player's input record as this reads it: its controller. */
typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 port;
    /* 0x02 */ u8 pad2[0x48 - 0x2]; void *ref;
} InputRec; /* 0x4C */


extern InputRec gPlayers[];

/* Whether port's pak is a Rumble Pak. */
static s8 controller_present(u8 port)
{
    return gPaks[port].rumble;
}

/* Hides the dimming blit when every player's controller has a Rumble Pak, shows it otherwise. Returns 1. */
s32 animControllerDimInternal(tBlit *spr)
{
    s32 i;
    s32 flag;

    flag = 1;
    for (i = 0; i < gNumPlayers; i++) {
        if (controller_present(gPlayers[i].port) == 0) {
            flag = 0;
        }
    }
    if (flag != spr->hide) {
        spr->hide = flag;
        UpdateBlit(spr);
    }
    return 1;
}
