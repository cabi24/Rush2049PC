#include "game/game.h"

typedef struct Player76 { u8 slot; u8 pad01[3]; s32 edges; u8 pad08[0x48 - 0x8]; void *ref; } Player76;
/* PC: a target is a level-object node, which keeps its N64 layout in the arena: the link is a 32-bit window address
 * (the node pool's n64 flag), so the sound and the state are at +0x60 and +0x64 (docs/port/POINTERS.md 10). */
typedef struct Target { u32 next; u8 pad04[92]; s32 f96; s8 f100; } Target;
#define TARGET_NEXT(t) ((Target *)PC_N64P((t)->next))

extern Player76 gPlayers[];   /* gPlayers */

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
                if (!(gGameState & GS_COUNTDOWN)) StartGameMusic();
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
                    if (gGameState & GS_RACING) goto retire_game;
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
                if ((gGameState & GS_RACING) && gPauseState == 8) {
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
