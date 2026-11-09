#include "game/game.h"

typedef struct InputRecord {
    u8 pad00[4];
    s32 unk04;
    u8 pad08[0x48 - 0x8]; void *ref;
} InputRecord;

extern InputRecord gPlayers[];

VIEW(s8, circuitCode);
#define circuitCode circuitCode_view

s32 CountdownTimeOut(s32 arg0);

void ControlUnlocked(void)
{
    s32 i;
    s32 done;

    done = 1;
    for (i = 0; i < gNumPlayers; i++) {
        if (gStuffUnlocked[i] == 1) {
            if ((gPlayers[i].unk04 & 3) || CountdownTimeOut(gPlayers[i].unk04)) {
                PlayStartABSound(gPlayers[i].unk04);
                gStuffUnlocked[i] = 0;
            }
        } else if (namePanes[i] != 0) {
            RemovePane(namePanes[i]);
            namePanes[i] = 0;
        }
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (gStuffUnlocked[i] == 1) {
            done = 0;
        }
    }
    if (done == 1) {
        if (circuitCode) {
            SetHighScoreState(2);
        } else if (gRace == 0 || gRace == 3 || gRace == 2) {
            SetHighScoreState(3);
        } else {
            SetHighScoreState(0);
        }
    }
}

