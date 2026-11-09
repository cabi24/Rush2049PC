/* ControlUnlocked: the input of the high-score screen's "unlocked" page (MAT3 highscorescr.c ControlUnlocked). */
#include "game/game.h"

/* A player's pad record (MAT3 tPadState, gPlayers). */
typedef struct InputRecord {
    u8 pad00[4];
    s32 edges;              /* 0x04 the buttons newly pressed */
    u8 pad08[0x48 - 0x8]; void *ref;
} InputRecord;

extern InputRecord gPlayers[];

/* Each player who unlocked something reads its notice until Start or A (or the page's time runs out); a player
 * with nothing (left) to read has its box removed. When nobody is reading, the next page: the circuit's code if
 * there is one, else name entry after a single, circuit or ghost race, else the end of the screen. */
void ControlUnlocked(void)
{
    s32 i;
    s32 done;

    done = 1;
    for (i = 0; i < gNumPlayers; i++) {
        if (gStuffUnlocked[i] == 1) {
            if ((gPlayers[i].edges & BUTTON_ACCEPT) || CountdownTimeOut()) {
                PlayStartABSound(gPlayers[i].edges);
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
            SetHighScoreState(kHighScoreCircuitCode);
        } else if (gRace == kOneRace || gRace == kCircuit || gRace == kGhost) {
            SetHighScoreState(kHighScoreNames);
        } else {
            SetHighScoreState(kHighScoreNone);
        }
    }
}
