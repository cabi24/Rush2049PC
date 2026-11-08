/* updateHighScoreScreen (0x800F64D4, 976 B): results-screen / save-ghost state machine (the label is
 * historical). If the screen is not yet set up, loads it; a pending abort returns to state 0. Then
 * switch on D_80140B20: 1 ControlUnlocked; 2 per-player "press to continue" (func_800F1210(3));
 * 3 per-player name entry (ControlName) and slot cleanup, then either the save prompt
 * (func_800F1210(4)) or the records table (5); 4 save input (ControlGhost); 5 records table:
 * toggle the shown list (gTable) and redraw it (func_800F0914), or continue / leave.
 * State changes go through func_800F1210, whose new state is a register parameter ($s2).
 *
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * changes:
 *  - the state-2 loop uses its own counter `j` (retail gives the two loops different webs:
 *    state 2 counter s7 / record pointer s6, state 3 counter s6 / slot pointer s7);
 *  - state 3 clears a slot through `h = &D_80149418[i]` (same code as the indexed form);
 *  - SHAPING (disclosed): eight unused s32 locals u1..u8. They are the exact frame residual:
 *    retail's frame is 144 bytes with named-local slots down to sp+96 (flag at sp+112, the
 *    state-2 counter's home at sp+100); every function-level named local takes one slot, used or
 *    not, and these eight put flag and j at retail's offsets. Without them the code is identical
 *    but the frame is 104 bytes (13 rows: frame, homes).
 * Its frame and register choice also depend on the IPA clobber sets of its internal callees: the
 * group's context func_800F0914 (clobbers s0-s8 and $f20/$f22, as retail) and func_800F1210. */
typedef signed int s32;
typedef signed short s16;
typedef signed char s8;
typedef unsigned int u32;

/* 76-byte records at gPlayers */
typedef struct Rec76 { s32 w0; u32 w4; s32 rest[16]; void *ref; } Rec76;

extern s8 highscore_inited;
extern s8 gSoftReset;
extern s32 gHighScoreState;              /* state, dispatched as state-1 (5 cases) */
extern s16 gNumPlayers;              /* record count */
extern Rec76 gPlayers[];
extern s8 gDrawName[];
extern s8 *namePanes[];
extern s32 gRace;
extern intptr_t gGhostToSave;
extern s8 gTrackNum;
extern void *gSavedGhost;
extern s8 gSaveGhostFiles;
/* PC: game.ghost_prompt (port/config/ghost_prompt.c, docs/port/SAVES.md "The ghost save prompt"): the save prompt
   after a ghost race for every profile, not only the developer's (gSaveGhostFiles, LoadPlayerData); off, the N64's. */
int pc_ghost_prompt(int developer);
#define PC_GHOST_PROMPT(v) pc_ghost_prompt(v)
extern s8 gGhostPak;
extern void *gPlayers_save;
extern u32 gActiveEdges;
extern s8 gTable;

extern void highscore_InitScreen(void);
extern void SaveServer(s32, s32);
extern void SaveFlush(s32);
extern void SetHighScoreState(s32);
extern void ControlUnlocked(void);
extern s32 CountdownTimeOut(void);
extern void PlayStartABSound(u32);
extern void ControlName(s32);
extern void RemovePane(s8 *);
extern void UnloadWaveAll(s32);
extern char gBlocksNeeded[];
extern void *CompressGhost(intptr_t, char *);
extern s32 SaveGhost(s32, void *, void *, s32);
extern void ControlGhost(void);
extern void PlayLRSound(u32);
extern void PrintScores(void);
extern void SetCountdownTimer(float);

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
