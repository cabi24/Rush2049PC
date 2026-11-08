/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* updatePauseScreen (0x800DB81C) = MAT3 pausescr.c updatePauseScreen (GC 0x80077AA8). NOT A MATCH:
 * 524/537 words, every other word identical; missing are retail's 13 `li t0,2` reloads (constant-2 web live over the
 * whole switch in retail, only PauseTargets + end chain here). Score with the whole-program unit (game unit, kept).
 * Levers: audio commands = kept functions over internal AudLock/AudUnlock/AudSend (inlined at all 5 sites, spill homes
 * 244/180/132/100/68 = retail); AudSend `if (osSendMesg(...)) {}` (retail's stray beqz v0 in the retire path; disclosed);
 * PauseGameMusic / PlaySelectSfx / SetPausePlayer kept wrappers (inline slots: frame 272 = retail; the choice of which
 * calls are wrappers is not unique); `pressed = (edges & 1) || !attached || 0;`; gControllerData volatile.
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10. */
/* updatePauseScreen (0x800DB81C) = MAT3 pausescr.c updatePauseScreen (GC 0x80077AA8, size 0x3D0). */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Player76 { u8 slot; u8 pad01[3]; s32 edges; u8 pad08[0x48 - 0x8]; void *ref; } Player76;
typedef struct Controller16 { s8 attached; u8 pad[15]; } Controller16;
/* PC: a target is a level-object node, which keeps its N64 layout in the arena: the link is a 32-bit window address
 * (the node pool's n64 flag), so the sound and the state are at +0x60 and +0x64 (docs/port/POINTERS.md 10). */
typedef struct Target { u32 next; u8 pad04[92]; s32 f96; s8 f100; } Target;
#define TARGET_NEXT(t) ((Target *)PC_N64P((t)->next))
typedef struct AudMsg { u8 pad0[2]; s8 type; } AudMsg;

extern s32 gPauseState;          /* gPauseState */
extern s32 pauseMenuSelect;          /* pauseMenuSelect */
extern s32 playerInPause;          /* playerInPause */
extern s16 gNumWindows;
extern Player76 gPlayers[];   /* gPlayers */
extern volatile Controller16 gControllerData[]; /* gControllerData */
extern s8 pause_gControllerRemoved;           /* gControllerRemoved */
extern s8 pause_gYes;           /* gYes */
extern s8 gRestartGame;           /* gRestartGame */
extern s8 gRetireGame;           /* gRetireGame */
extern f32 gModelTime;          /* gModelTime */
extern s8 gSoftReset;           /* gSoftReset */
extern u32 gGameState;        /* gGameState */
extern Target *gTargetList_head;      /* target list */
extern s8 controlScrActive[];         /* control screen entered from pause, per player */
extern u8 audio_lock_queue[];         /* audio lock queue */
extern u8 audio_command_queue[];         /* audio command queue */

void SaveServer(s32 forceUpdate, s32 rumbleOnly);
void setPauseState(s32 state);          /* setPauseState */
void PauseTimer(void);               /* PauseTimer */
void ResumeTimer(void);              /* ResumeTimer */
AudMsg *GetCmd(void);
s32 osRecvMesg(void *mq, void *msg, s32 flag);
s32 osSendMesg(void *mq, void *msg, s32 flag);
void AudSetMusicVol(f32 vol, f32 time); /* AudSetMusicVol */
void StopListeners(void);               /* StopListeners */
void modelstop(void);        /* modelstop */
void StartListeners(void);               /* StartListeners */
void modelgo(void);               /* modelgo */
void StartGameMusic(void);          /* StartGameMusic */
void ResumeGameMusic(void);          /* ResumeGameMusic */
void SetControllerStickEdges(s32 player, s32 value); /* SetControllerStickEdges */
void SaveFlush(s32 action); /* SaveFlush */
u32 AudPlayEffect(u32 fx, u32 sub, u32 replace, u8 prio); /* AudPlayEffect */
void updateOptionScreen(void);               /* updateOptionScreen */
void updateControlScreen(void);               /* updateControlScreen */
void updateAudioScreen(void);               /* updateAudioScreen */
void updateVideoScreen(void);                 /* updateVideoScreen */
void PlayUDSound(s32 edges);   /* PlayUDSound */
void PlayLRSound(s32 edges);          /* PlayLRSound */
void PlayStartABSound(s32 edges);   /* PlayStartABSound */
void AbortGame(void);               /* AbortGame */
void pause_TitlePane(void);

/* The game's own audio commands and PauseGameMusic (this file had copies of each, with their lock and send
 * helpers: the same code). */
void AudPauseAllEffects(void);
void AudResumeAllEffects(void);
void AudStopAllEffects(void);
void PauseGameMusic(void);

void PlaySelectSfx(void)
{
    AudPlayEffect(37, 0, 1, 0);
}

void PlayPauseSfx(void)
{
    AudPlayEffect(46, 0, 1, 0);
}

void SetPausePlayer(s32 player)
{
    playerInPause = player;
    pause_gControllerRemoved = !gControllerData[playerInPause].attached;
}

void PauseTargets(void);   /* the game's own PauseTargets (this file had a static copy: the same code) */

void updatePauseScreen(void)
{
    s32 i;
    s32 pressed;

    if (gPauseState) SaveServer(0, 0);
    switch (gPauseState) {
    case 0:
        for (i = 0; i < gNumWindows; i++) {
            pressed = (gPlayers[i].edges & 1) || !gControllerData[i].attached || 0;
            if (pressed) {
                SetPausePlayer(i);
                goto pause_game;
            }
        }
        return;
    pause_game:
        setPauseState(1);
        pauseMenuSelect = 0;
        PauseTimer();
        AudPauseAllEffects();
        PauseGameMusic();
        StopListeners();
        modelstop();
        PauseTargets();
        SetControllerStickEdges(-1, 1);
        SaveFlush(1);
        AudPlayEffect(46, 0, 1, 0);
        break;
    case 5: updateOptionScreen(); break;
    case 4: updateControlScreen(); break;
    case 2: updateAudioScreen(); break;
    case 3: updateVideoScreen(); break;
    case 1:
        if (gPlayers[playerInPause].edges & 2) {
            PlaySelectSfx();
            switch (pauseMenuSelect) {
            case 0:
            resume_game:
                setPauseState(0);
                ResumeTimer();
                StartListeners();
                AudResumeAllEffects();
                ResumeGameMusic();
                if (!(gGameState & 0x200000)) StartGameMusic();
                modelgo();
                SetControllerStickEdges(-1, 0);
                return;
            case 6:
                if (gModelTime < 15.0f) {
                    gRestartGame = 1;
                    goto abort_game;
                } else setPauseState(7);
                break;
            case 7:
                if (gModelTime < 15.0f) {
                    gRestartGame = 0;
                    if (gGameState & 0x400000) goto retire_game;
                    goto abort_game;
                } else setPauseState(8);
                break;
            case 3: setPauseState(2); break;
            case 4: setPauseState(3); break;
            case 1: setPauseState(5); break;
            case 2:
                controlScrActive[playerInPause] = 1;
                setPauseState(4);
                break;
            case 5: setPauseState(6); break;
            }
        } else if (gPlayers[playerInPause].edges & 0x400) {
            PlayUDSound(gPlayers[playerInPause].edges);
            if (--pauseMenuSelect < 0) pauseMenuSelect = 7;
        } else if (gPlayers[playerInPause].edges & 0x800) {
            PlayUDSound(gPlayers[playerInPause].edges);
            if (++pauseMenuSelect >= 8) pauseMenuSelect = 0;
        } else if (gPlayers[playerInPause].edges & 5) {
            PlayStartABSound(gPlayers[playerInPause].edges);
            goto resume_game;
        }
        break;
    case 7: case 8:
        if (gPlayers[playerInPause].edges & 0x3000) {
            pause_gYes = !pause_gYes;
            PlayLRSound(gPlayers[playerInPause].edges);
        }
        if (gPlayers[playerInPause].edges & 2) {
            if (pause_gYes) {
                if ((gGameState & 0x400000) && gPauseState == 8) {
                retire_game:
                    gRetireGame = 1;
                    setPauseState(0);
                    ResumeTimer();
                    AudStopAllEffects();
                } else {
                    gRestartGame = gPauseState == 7;
                abort_game:
                    setPauseState(0);
                    ResumeTimer();
                    AudStopAllEffects();
                    AbortGame();
                }
            } else setPauseState(1);
        } else if (gPlayers[playerInPause].edges & 5) {
            PlayStartABSound(gPlayers[playerInPause].edges);
            setPauseState(1);
        }
        break;
    case 6:
        if (gPlayers[playerInPause].edges & 7) {
            PlayStartABSound(gPlayers[playerInPause].edges);
            setPauseState(1);
        }
        break;
    }
    if (gPauseState) {
        if (!gControllerData[playerInPause].attached != pause_gControllerRemoved) {
            pause_gControllerRemoved = !gControllerData[playerInPause].attached;
            if (gPauseState != 5 && gPauseState != 4 && gPauseState != 2 && gPauseState != 3) pause_TitlePane();
        }
    }
    if (gSoftReset) {
        gRestartGame = 0;
        setPauseState(0);
        ResumeTimer();
        AudStopAllEffects();
        AbortGame();
    }
}
