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

VIEW(u8, gBlocksNeeded);
#define gBlocksNeeded gBlocksNeeded_view

/* PC: both runtime images are linked in, and image B has a function at 0x8038A400 too (findDamageSlot, worlddam.c),
 * which the loader's tables name for both. Image A's is updateWarningScreen, which updateFrontEnd calls in
 * state 0x40000000 and port/data's function table gives image A's data (docs/port/SYNTH.md). Its two file statics
 * are read by drawWarningScreen (warningscr_animText) by their addresses, so they are the image A arena's bytes. */
#define numPaksFound num_paks_found

/* The warning's blit while it is up (MAT3's function-local static; image A's data: a load of the image clears it,
 * docs/port/DATA.md 5). */
Blit *warningscr_blit = 0;

void updateWarningScreen(void)
{
    s32 numPaks;
    s32 i;

    if (!warningscr_blit) {
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
                if (!warningscr_blit) {
                    warningscr_blit = NewMultiBlit(0, 0, warningscr_blits, 1);
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
    if (warningscr_blit) {
        RemoveBlit(warningscr_blit);
        warningscr_blit = 0;
    }
    gBlocksNeeded = PS_GetBlocks(0x810);
    setGameState(gPakScr ? 0x4000000 : 4);
}
