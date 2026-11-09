#include "game/game.h"

typedef struct Blit Blit;
/* PC: WarningBlit is an array, which gcc wants complete: the MultiBlit of the other image-A screens. */
typedef struct MultiBlit {
    const char *texname;
    short dulx, duly, width, height, top, bot, left, right;
    u32 zdepth, alpha;
    s32 (*animfunc)(Blit *);
    u32 animid;
} MultiBlit;
typedef struct Controller { s8 attached; u8 rest[15]; } Controller;

struct PlayerFiles { u32 unknown0,  unknown4 ; void * head,  * tail ; };
VIEW(struct PlayerFiles, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view
VIEW(Controller[4], gControllerData);
#define gControllerData gControllerData_view
VIEW(u8, gBlocksNeeded);
#define gBlocksNeeded gBlocksNeeded_view
VIEW(MultiBlit[1], warningscr_blits);
#define warningscr_blits warningscr_blits_view

extern Blit *NewMultiBlit(s16, s16, MultiBlit *, s16);
extern void RemoveBlit(Blit *);

/* PC: both runtime images are linked in, and image B has a function at 0x8038A400 too (findDamageSlot, worlddam.c),
 * which the loader's tables name for both. Image A's is updateWarningScreen, which updateFrontEnd calls in
 * state 0x40000000 and port/data's function table gives image A's data (docs/port/SYNTH.md). Its two file statics
 * are read by drawWarningScreen (warningscr_animText) by their addresses, so they are the image A arena's bytes. */
#define numPaksFound num_paks_found

void updateWarningScreen(void)
{
    static Blit *sBlit = 0;       /* D_803B83C8 */
    s32 numPaks;
    s32 i;

    if (!sBlit) {
        gPakScr = 0;
        pak_recognize_error_seen = 0;
    }
    if (!gPlayerFiles.head) {
        for (i = 0; i < 4; i++) {
            if (gControllerData[i].attached) break;
        }
        if (i < 4) {
            numPaks = 0;
            for (i = 0; i < 4; i++) {
                if (PS_PakInserted(i) && !PS_PakDamaged(i) && !PS_PakRumble(i)) {
                    numPaks++;
                    if (PS_GetFreeBlocks(i) >= PS_GetBlocks(0x810) && PS_GetFreeFiles(i) >= 1) goto done;
                }
            }
            numPaksFound = numPaks;
            if (numPaksFound > 0 || !pak_recognize_error_seen) {
                if (!sBlit) {
                    sBlit = NewMultiBlit(0, 0, warningscr_blits, 1);
                    SetControllerActive(-1, 1);
                    SetControllerStickEdges(-1, 1);
                }
                if (numPaksFound > 0) {
                    if (gActiveEdges & 0xc00) {
                        PlayUDSound(gActiveEdges);
                        gPakScr = !gPakScr;
                    }
                } else {
                    gPakScr = 0;
                }
                if (!(3 & gActiveEdges)) return;
                PlayStartABSound(gActiveEdges);
            }
        }
    }
done:
    if (sBlit) {
        RemoveBlit(sBlit);
        sBlit = 0;
    }
    gBlocksNeeded = PS_GetBlocks(0x810);
    setGameState(gPakScr ? 0x4000000 : 4);
}
