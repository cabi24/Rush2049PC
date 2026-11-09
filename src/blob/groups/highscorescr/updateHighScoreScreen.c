#include "game/game.h"

/* 76-byte records at gPlayers */
typedef struct Rec76 { s32 w0; u32 w4; s32 rest[16]; void *ref; } Rec76;

VIEW(s8, highscore_inited);
#define highscore_inited highscore_inited_view

extern Rec76 gPlayers[];

VIEW(intptr_t, gGhostToSave);
#define gGhostToSave gGhostToSave_view

/* PC: game.ghost_prompt (port/config/ghost_prompt.c, docs/port/SAVES.md "The ghost save prompt"): the save prompt
   after a ghost race for every profile, not only the developer's (gSaveGhostFiles, LoadPlayerData); off, the N64's. */
int pc_ghost_prompt(int developer);
#define PC_GHOST_PROMPT(v) pc_ghost_prompt(v)

VIEW(void *, gPlayers_save);
#define gPlayers_save gPlayers_save_view
VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view

extern s32 CountdownTimeOut(void);

extern void *CompressGhost(intptr_t, char *);

void updateHighScoreScreen(void)
{
    s8 **h;
    s32 u1, u2, u3, u4, u5, u6;
    s32 flag;
    s32 u7, u8;
    s32 j;
    s32 i;

    if (highscore_inited == 0) {
        highscore_InitScreen();
        SaveServer(1, 0);
        SaveFlush(0);
    }
    if (gSoftReset != 0) {
        SetHighScoreState(0);
        return;
    }
    switch (gHighScoreState) {
    case 1:
        ControlUnlocked();
        break;
    case 2:
        for (j = 0; j < gNumPlayers; j++) {
            if ((gPlayers[j].w4 & 3) != 0 || CountdownTimeOut() != 0) {
                PlayStartABSound(gPlayers[j].w4);
                SetHighScoreState(3);
            }
        }
        break;
    case 3:
        flag = 1;
        for (i = 0; i < gNumPlayers; i++) {
            if (gDrawName[i] == 1) {
                ControlName(i);
                flag = 0;
            } else {
                h = &namePanes[i];
                if (*h != 0) {
                    RemovePane(*h);
                    *h = 0;
                }
            }
        }
        if (flag == 1) {
            if (gRace == 2 && gGhostToSave != 0) {
                UnloadWaveAll(gTrackNum + 139);
                gSavedGhost = CompressGhost(gGhostToSave, gBlocksNeeded);
                gGhostToSave = 0;
                if (PC_GHOST_PROMPT(gSaveGhostFiles) != 0) {
                    SetHighScoreState(4);
                } else {
                    SaveGhost(gGhostPak, gPlayers_save, gSavedGhost, 1);
                    gSavedGhost = 0;
                    SetHighScoreState(5);
                }
            } else {
                SetHighScoreState(5);
            }
        }
        break;
    case 4:
        SaveServer(0, 0);
        ControlGhost();
        break;
    case 5:
        if (gRace != 2 && (gActiveEdges & 0x3000) != 0) {
            PlayLRSound(gActiveEdges);
            if (gTable == 0) gTable = 1; else gTable = 0;
            PrintScores();
            SetCountdownTimer(15.0f);
        } else if ((gActiveEdges & 3) != 0 || CountdownTimeOut() != 0) {
            if (gRace != 2 && CountdownTimeOut() != 0 && gTable == 0) {
                gTable = 1;
                PrintScores();
                SetCountdownTimer(15.0f);
            } else {
                PlayStartABSound(gActiveEdges);
                SetHighScoreState(0);
            }
        }
        break;
    }
}
