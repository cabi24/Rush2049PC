/* GameLoop: one turn of the game thread's loop (MAT3 game.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { s32 opaque[6]; } OSMesgQueue;


/* One frame of the game: a change of game state starts a new frame's graphics (waiting for the last to finish
 * drawing) and reads the controls; the idle warning after 5 minutes without a key (but in the attract, demo and
 * front-end states 0x4000000F); the state's own update (updateGameState), the front end's screens, the pause screen
 * in a race (paused: only the blits, the environment and the cameras drawn), the race (UpdateGame); then, the state
 * unchanged, the frame is drawn. Returns 1 if a frame was drawn. */
s32 GameLoop(void)
{
    s32 renderFlag;
    f32 elapsed;
    static s8 sInited = 0;

    renderFlag = 0;
    if (!sInited) {
        frame_counter = 0;
        sInited = 1;
    }
    if (gNewGameState == gGameState) {
        UpdateFrame();
    } else {
        while (gRenderInProgress) { pc_os_spin_yield("GameLoop");
        }
        blit_dl = 0; pc_game_fps_begin(); pc_game_fps_state(gNewGameState);
        StartGraphics();
        frame_dl_start = (u8 *) frame_dl;
        UpdateRushControls();
    }
    if ((elapsed = (f32)(u32)(gScheduler.retraceCount - gLastKeyTime) * gRetraceTime) > 300.0f && !(gGameState & (GS_WARNING | GS_DEMO | GS_TITLE | GS_LICENSE | GS_BOOT))) {
        if (!inactiveBlit) {
            inactiveBlit = NewMultiBlit(0, 0, idle_warning_multiblit, 1);
        }
    } else if (inactiveBlit) {
        RemoveBlit(inactiveBlit);
        inactiveBlit = 0;
    }
    updateGameState();
    if (gGameState & GS_FRONTEND_MASK) {
        updateFrontEnd();
    }
    if ((gGameState & GS_COUNTDOWN) || (gGameState & GS_RACING)) {
        updatePauseScreen();
        if (gPauseState) {
            DrawBlits();
            UpdateEnvirons();
            UpdateCameras();
            ZOID_EndFrame(0);
            return 1;
        }
    }
    if (gGameState & GS_IN_GAME_MASK) {
        UpdateGame();
    }
    if (gNewGameState == gGameState) {
        DrawNoControllers();
        DrawBlits();
        UpdateEnvirons();
        UpdateCameras();
        ZOID_EndFrame(0);
        renderFlag = 1;
    }
    frame_counter++;
    return renderFlag;
}
