#include "game/game.h"

typedef struct Player {
    u8 pad00;
    u8 controller;        /* 0x01 */
    u8 pad02[0x18 - 2];
    u32 controls[10];     /* 0x18 */
    u8 pad40[0x48 - 0x40];
    void **save;          /* 0x48 */
} Player;

extern Player gPlayers[];         /* gPlayers */
VIEW(s8[10], gDefaultControls);
#define gDefaultControls gDefaultControls_view

VIEW(void *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view

void SetControls(s32 resetToDefault)
{
    s32 p;
    s8 *controls;
    for (p = 0; p < gNumPlayers; p++) {
        if (resetToDefault) {
            controls = gDefaultControls;
        } else {
            controls = gNewControls[gPlayers[p].controller];
        }
        if (gPlayers[p].controller != 5 && gControlsSet[gPlayers[p].controller]) {
            gControlsSet[gPlayers[p].controller] = 0;
            if (gPlayers[p].save) SavePlayerControls(gPlayers[p].save, controls);
            if (!gPlayers[p].save || resetToDefault) SavePlayerControls(&gJustPlayers[gPlayers[p].controller], controls);
        }
        {
            s32 i;
            for (i = 0; i < 10; i++) gPlayers[p].controls[i] = gControlTable[controls[i]];
        }
    }
}
