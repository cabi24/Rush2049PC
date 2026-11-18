#include "game/game.h"

/* Image A's first function. Image B's first function (findDamageSlot, worlddam.c) has the same place in its image,
 * and the cartridge's symbol tables give both that one name; both images are linked into the program, so image A's
 * is compiled under its MAT3 name. updateFrontEnd calls it in the pak warning's state (GS_WARNING), and the data
 * loader's function table gives image A's entry this function (docs/port/SYNTH.md). MAT3's file statics gPakScr and
 * numPaksFound are image A's data (frontend.h), since drawWarningScreen (warningscr_animText) reads them too. */
#define numPaksFound num_paks_found

/* The warning's blit while it is up (MAT3's function-local static; image A's data: a load of the image clears it,
 * docs/port/DATA.md 5). */
tBlit *warningscr_blit = 0;

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
    gBlocksNeeded = PS_GetBlocks(PROFILE_BYTES);
    setGameState(gPakScr ? 0x4000000 : 4);
}
