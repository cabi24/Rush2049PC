/* The player (profile select) screen's tear-down (MAT3 playerscr.c uninitScreen and uninitButtons). */
#include "game/game.h"

typedef struct Blit Blit;typedef struct FiveParts FiveParts;

/* RemovePane, under the stale name image A's symbol table still has for it (symbol_addrs.us.txt) */
extern void sound_handles_array_clear ( FiveParts * );

/* Take the screen down: every player's buttons and pane, the controls screen, the text blit and the background; the
 * view goes back to one window with the usual 90-degree field of view, and the scene is reset. */
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

/* Remove player p's buttons (3D models) from the scene and mark them gone (handle -1). */
void playerscr_uninitButtons(s8 p)
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < PLAYER_BUTTONS; buttonIdx++) {
        if (playerscr_buttonInfo[p][buttonIdx].handle != -1) {
            ZOID_RemoveObject(playerscr_buttonInfo[p][buttonIdx].handle);
            playerscr_buttonInfo[p][buttonIdx].handle = -1;
        }
    }
    playerscr_buttonsInited[p] = 0;
}
