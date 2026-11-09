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

extern Blit *NewMultiBlit(short, short, const MultiBlit *, short);
extern void RemoveBlit(Blit *);

extern s32 CountdownTimeOut(void);

void updateLicenseScreen(void)
{
    s8 pressed;
    s32 i;

    if (!pfsInited) {
        pressed = 0;
        for (i = 0; i < 4; i++) {
            pressed |= (gContLevels[i] & 1) != 0;
        }
        PS_InitPakSys(PS_ErrorFunc);
        PS_PakServer(1, 0);
        if (!lcdInited) {
            lcdInited = 1;
        }
        pfsInited = 1;
        if (pressed) {
            gBlocksNeeded = PS_GetBlocks(0x810);
            setGameState(0x04000000);
            return;
        }
    }
    if (!licensescr_inited) {
        licensescr_blit = NewMultiBlit(0, 0, licensescr_blits, 2);
        licensescr_inited = 1;
        SetCountdownTimer(3.0f);
    }
    if (CountdownTimeOut() || (gLicenseShown && (gActiveEdges & 7))) {
        if (licensescr_blit) {
            RemoveBlit(licensescr_blit);
            licensescr_blit = 0;
        }
        licensescr_inited = 0;
        setGameState(gLicenseShown ? 4 : 0x40000000);
        gLicenseShown = 1;
    }
}
