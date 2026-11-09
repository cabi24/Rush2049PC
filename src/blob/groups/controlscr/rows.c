/* The controller-setup screen's tear-down (MAT3 controlscr.c uninitButtons and cleanupControlScreen). */
#include "game/game.h"

VIEW(s8[4], prevControlScrActive);
#define prevControlScrActive prevControlScrActive_view

void RemoveBlit(intptr_t);

/* Remove player row's 17 buttons (3D models) from the scene and mark them gone (handle -1). */
void controls_uninitButtons(s8 row) {
    int i;
    MenuButton *record=controls_buttonInfo[row];
    /* The loop counts the bytes of the row, a button at a time. */
    for(i=0;i<sizeof(controls_buttonInfo[0]);i+=sizeof(MenuButton),record++){
        if(record->handle!=-1){
            RemoveObjectTree((s16)record->handle,0,0);
            record->handle=-1;
        }
    }
    controls_buttonsInited[row]=0;
}

/* Close the controls screen for every player: no player has it open, every player's buttons go, and so does its
 * text blit. */
void cleanupControlScreen(void) {
    int i;
    for(i=0;i<4;i++){
        controlScrActive[i]=0;
        prevControlScrActive[i]=0;
        controls_uninitButtons((s8)i);
    }
    if(controls_blit!=0){
        RemoveBlit(controls_blit);
        controls_blit=0;
    }
    controls_inited=0;
}
