#include "game/game.h"

typedef struct Blit Blit;typedef struct FiveParts FiveParts;

typedef struct Slot64 {
    s32 unknown0,id;
    f32 angle,matrix[3][3],position[3];
    s32 unknown3C;
} Slot64;
VIEW(Slot64[][22], playerscr_buttonInfo);
#define playerscr_buttonInfo playerscr_buttonInfo_view

extern void sound_handles_array_clear ( FiveParts * );
extern void RemoveBlit ( Blit * );

void playerscr_uninitScreen(void)
{
    s32 i;
    for(i=0;i<gNumPlayers;i++) {
        playerscr_uninitButtons(i);
        if(playerscr_pane[i]) {
            sound_handles_array_clear(playerscr_pane[i]);
            playerscr_pane[i]=0;
        }
    }
    cleanupControlScreen();
    if(playerscr_blit) {
        RemoveBlit(playerscr_blit);
        playerscr_blit=0;
    }
    removeBG();
    HFOV=90.0f;
    gNumWindows=1;
    ZOID_ResetWindows();
    ZOID_Reset();
    screenInited=0;
}

/* uninitButtons@playerscr.c (MAT3 GC uninitButtons(signed char p)); deleted stub 0x803949C4 */
void playerscr_uninitButtons(s8 p)
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 22; buttonIdx++) {
        if (playerscr_buttonInfo[p][buttonIdx].id != -1) {
            ZOID_RemoveObject(playerscr_buttonInfo[p][buttonIdx].id);
            playerscr_buttonInfo[p][buttonIdx].id = -1;
        }
    }
    playerscr_buttonsInited[p] = 0;
}

