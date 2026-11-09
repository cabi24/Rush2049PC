/* updateHighScoreScreen: the high-score screen after a race (MAT3 highscorescr.c updateHighScoreScreen), GS_HIGH_SCORE.
 * Its pages, in order: what the race unlocked, the circuit's code, name entry, the ghost save menu, the tables. */
#include "game/game.h"

/* A player's pad record (MAT3 tPadState, gPlayers): 76 bytes, 80 on the PC. */
typedef struct Rec76 { s32 w0; u32 edges; s32 rest[16]; void *profile; } Rec76;

extern Rec76 gPlayers[];

VIEW(intptr_t, gGhostToSave);
#define gGhostToSave gGhostToSave_view

/* PC: game.ghost_prompt (port/config/ghost_prompt.c, docs/port/SAVES.md "The ghost save prompt"): the save prompt
   after a ghost race for every profile, not only the developer's (gSaveGhostFiles, LoadPlayerData); off, the N64's. */
int pc_ghost_prompt(int developer);
#define PC_GHOST_PROMPT(v) pc_ghost_prompt(v)

extern s32 CountdownTimeOut(void);

extern void *CompressGhost(intptr_t, char *);

#define WAVE_TRACK_MUSIC_BASE 139   /* the track's wave, gTrackNum + this (freed for the ghost's compression) */

/* The screen's frame. The first frame sets it up and saves the profiles (SaveServer, SaveFlush). A soft reset ends
 * it. Then by page:
 * - unlocked: ControlUnlocked;
 * - circuit code: any player's Start or A (or the time limit) goes on to name entry;
 * - names: each player who made a table enters a name (ControlName); when all are done, after a ghost race the
 *   recorded ghost is compressed (its track wave freed for room) and either offered to be saved (the ghost menu,
 *   for the developer's profile, or everyone's with game.ghost_prompt) or saved at once; then the tables;
 * - ghost: ControlGhost, with the profile saves held (SaveServer(0, 0));
 * - tables: left or right flips between the race's two tables (not after a ghost race); Start, A or the time limit
 *   ends the screen, but the time limit first shows the second table. */
void updateHighScoreScreen(void)
{
    s8 **h;
    s32 allNamed;
    s32 j;
    s32 i;

    if (highscore_inited == 0) {
        highscore_InitScreen();
        SaveServer(1, 0);
        SaveFlush(0);
    }
    if (gSoftReset != 0) {
        SetHighScoreState(kHighScoreNone);
        return;
    }
    switch (gHighScoreState) {
    case kHighScoreUnlocked:
        ControlUnlocked();
        break;
    case kHighScoreCircuitCode:
        for (j = 0; j < gNumPlayers; j++) {
            if ((gPlayers[j].edges & BUTTON_ACCEPT) != 0 || CountdownTimeOut() != 0) {
                PlayStartABSound(gPlayers[j].edges);
                SetHighScoreState(kHighScoreNames);
            }
        }
        break;
    case kHighScoreNames:
        allNamed = 1;
        for (i = 0; i < gNumPlayers; i++) {
            if (gDrawName[i] == 1) {
                ControlName(i);
                allNamed = 0;
            } else {
                h = &namePanes[i];
                if (*h != 0) {
                    RemovePane(*h);
                    *h = 0;
                }
            }
        }
        if (allNamed == 1) {
            if (gRace == kGhost && gGhostToSave != 0) {
                UnloadWaveAll(gTrackNum + WAVE_TRACK_MUSIC_BASE);
                gSavedGhost = CompressGhost(gGhostToSave, gBlocksNeeded);
                gGhostToSave = 0;
                if (PC_GHOST_PROMPT(gSaveGhostFiles) != 0) {
                    SetHighScoreState(kHighScoreGhost);
                } else {
                    /* the first player's profile (gPlayers_save is the label of this field) */
                    SaveGhost(gGhostPak, gPlayers[0].profile, gSavedGhost, 1);
                    gSavedGhost = 0;
                    SetHighScoreState(kHighScoreScores);
                }
            } else {
                SetHighScoreState(kHighScoreScores);
            }
        }
        break;
    case kHighScoreGhost:
        SaveServer(0, 0);
        ControlGhost();
        break;
    case kHighScoreScores:
        if (gRace != kGhost && (gActiveEdges & (BUTTON_LEFT | BUTTON_RIGHT)) != 0) {
            PlayLRSound(gActiveEdges);
            if (gTable == 0) gTable = 1; else gTable = 0;
            PrintScores();
            SetCountdownTimer(15.0f);
        } else if ((gActiveEdges & BUTTON_ACCEPT) != 0 || CountdownTimeOut() != 0) {
            if (gRace != kGhost && CountdownTimeOut() != 0 && gTable == 0) {
                gTable = 1;
                PrintScores();
                SetCountdownTimer(15.0f);
            } else {
                PlayStartABSound(gActiveEdges);
                SetHighScoreState(kHighScoreNone);
            }
        }
        break;
    }
}
