/* updateLicenseScreen: the licence screen at boot (MAT3 licensescr.c updateLicenseScreen), GS_LICENSE. On the
 * N64 it also starts the controller-pak system, and Start held on any controller at boot opens the pak manager. */
#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit {
    const char *texname;
    short dulx, duly, width, height, top, bot, left, right;
    u32 zdepth, alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;

VIEW(s8, gBlocksNeeded);
#define gBlocksNeeded gBlocksNeeded_view

VIEW(u32[4], gContLevels);
#define gContLevels gContLevels_view
VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view

VIEW(MultiBlit[], licensescr_blits);
#define licensescr_blits licensescr_blits_view

extern s32 CountdownTimeOut(void);

#define PROFILE_BYTES 0x810         /* a profile's save data (PS_GetBlocks: the pages it takes) */

/* The screen's frame. The first frame of the game starts the pak system (its error box PS_ErrorFunc) and, if Start
 * is held on any controller, goes to the pak manager (GS_PAK) with the pages a profile needs. Then the licence shows
 * for 3 seconds (a button of Start, A or B cuts it short once it has been seen); the first time it is followed by the
 * pak warning (GS_WARNING), later by the title. */
void updateLicenseScreen(void)
{
    s8 pressed;
    s32 i;

    if (!pfsInited) {
        pressed = 0;
        for (i = 0; i < 4; i++) {
            pressed |= (gContLevels[i] & BUTTON_START) != 0;
        }
        PS_InitPakSys(PS_ErrorFunc);
        PS_PakServer(1, 0);
        if (!lcdInited) {
            lcdInited = 1;
        }
        pfsInited = 1;
        if (pressed) {
            gBlocksNeeded = PS_GetBlocks(PROFILE_BYTES);
            setGameState(GS_PAK);
            return;
        }
    }
    if (!licensescr_inited) {
        licensescr_blit = NewMultiBlit(0, 0, licensescr_blits, 2);
        licensescr_inited = 1;
        SetCountdownTimer(3.0f);
    }
    if (CountdownTimeOut() || (gLicenseShown && (gActiveEdges & (BUTTON_ACCEPT | BUTTON_B)))) {
        if (licensescr_blit) {
            RemoveBlit(licensescr_blit);
            licensescr_blit = 0;
        }
        licensescr_inited = 0;
        setGameState(gLicenseShown ? GS_TITLE : GS_WARNING);
        gLicenseShown = 1;
    }
}
