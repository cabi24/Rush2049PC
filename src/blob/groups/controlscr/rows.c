#include "game/game.h"

typedef struct Item { int type, handle; float angle, matrix[3][3], position[3]; u8 alpha, pad[3]; } Item;
VIEW(Item[4][17], controls_buttonInfo);
#define controls_buttonInfo controls_buttonInfo_view
VIEW(int[4], controls_buttonsInited);
#define controls_buttonsInited controls_buttonsInited_view

VIEW(s8[4], controlScrActive);
#define controlScrActive controlScrActive_view
VIEW(s8[4], prevControlScrActive);
#define prevControlScrActive prevControlScrActive_view

void RemoveBlit(intptr_t);
void controls_uninitButtons(s8 row) {
    int i;
    Item *record=controls_buttonInfo[row];
    /* Native loop counter measures the actual row span in bytes. */
    for(i=0;i<sizeof(controls_buttonInfo[0]);i+=sizeof(Item),record++){
        if(record->handle!=-1){
            RemoveObjectTree((s16)record->handle,0,0);
            record->handle=-1;
        }
    }
    controls_buttonsInited[row]=0;
}
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
/* note: controls_uninitButtons = uninitButtons@controlscr.c (GC 0x8006D1D8; p in s4, private ABI),
 * cleanupControlScreen = cleanupControlScreen (GC 0x8006BB9C). Bodies, unchanged. */
