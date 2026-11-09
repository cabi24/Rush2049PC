/* RemoveHUD: remove the race's HUD (MAT3 hud.c). */
#include "game/game.h"

#define kStunt 4      /* gRace (MAT3 enum kNumRaceTypes) */
#define kBattle 6

/* A player's two coin counters' 3D coins (gPlayerCoin; CoinSlot's layout): the objects' ids. */
typedef struct RecordSlot { s16 id; u8 pad2[50]; } RecordSlot;       /* 52 bytes */
typedef struct RecordRow { RecordSlot a, b; } RecordRow;              /* 104 bytes */

VIEW(intptr_t[], resultPane);
#define resultPane resultPane_view

VIEW(RecordRow[], gPlayerCoin);
#define gPlayerCoin gPlayerCoin_view

/* Removes the HUD ShowHUD made: the panes, the HUD's multiblit (HudBlit1) and each window's result pane and coin
 * objects, the second multiblit (HudBlit2), and in battle and stunt races the extras (RemoveHUDExtra). */
void RemoveHUD(void) {
    s32 i;
    s32 id;
    s32 neg;
    RecordRow *row;
    intptr_t *status;   /* resultPane is pointer-sized on the PC */

    if (HudBlit1 != 0) {
        RemovePanes(1);
        RemoveBlit(HudBlit1);
        HudBlit1 = 0;
        neg = -1;
        i = 0;
        if (gNumWindows > 0) {
            status = resultPane; row = gPlayerCoin;
            do {
                id = row->a.id;
                *status = 0;
                if (neg != id) {
                    RemoveObjectTree((s16)id, 0, 0);
                    row->a.id = neg;
                }
                id = row->b.id;
                if (neg != id) {
                    RemoveObjectTree((s16)id, 0, 0);
                    row->b.id = neg;
                }
                i++;
                status++;
                row++;
            } while (i < gNumWindows);
        }
        resultPane4 = 0;
    }
    if (HudBlit2 != 0) {
        RemoveBlit(HudBlit2);
        HudBlit2 = 0;
    }
    if (gRace == kBattle || gRace == kStunt) {
        RemoveHUDExtra();
    }
}
