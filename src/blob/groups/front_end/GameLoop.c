/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * GameLoop = GameLoop@game.c (MAT3 GameSource/src/game.c GameLoop, GC 0x8001407C; N64 0x800FD464, 704 B).
 * N64 form: OncePerFrame/getGameStateUpdate become `gGameState == gNewGameState ? game_mode_handler() : (wait for
 * the retrace flag, blit_dl = 0, StartGraphics, frame_dl_start = frame_dl, UpdateRushControls)`; the controller-warning
 * blit (_PAD_ScrollBlit-like, inactiveBlit with multiblit idle_warning_multiblit) is created when more than 300 (scaled
 * retrace ticks) passed since gLastKeyTime and gGameState has none of 0x4000000F, else removed; updateGameState =
 * updateGameState, updateFrontEnd = updateFrontEnd, updatePauseScreen = updatePauseScreen (pause:
 * gPauseState gPauseState -> DrawBlits/UpdateEnvirons/UpdateCameras/
 * ZOID_EndFrame(0) and return 1), _UpdateGame = UpdateGame, _DrawNoControllers = DrawNoControllers.
 * gGameState = gGameState, gNewGameState = gNewGameState, gFrames = frame_counter (volatile: retail lui/addiu +
 * 0(reg) at both accesses), sInited = function-local static (0x801146EC, own .data, game.c's TU), GVSyncCount =
 * the volatile scheduler clock gScheduler.tick (house type from animDot.c).
 * Saves s0-s8/f20/f22 and keeps renderFlag at 84(sp) only with UpdateGame + init_state_continue internal in the
 * program (they use s0-s8 unsaved): their sources are context here (byte copies of update_game).
 * Levers: `gNewGameState == gGameState` operand order at both tests (MAT3 spelling), the elapsed time a
 * named f32 (product in $f0) assigned inside the if condition (a separate statement line flips the as1 order of
 * `lui t6` / `lui at,0x4f80`), static sInited (an extern gives a CSE'd address).  No unused locals, no stand-ins.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { s32 opaque[6]; } OSMesgQueue;

extern volatile s32 frame_counter;      /* gFrames */
extern void *inactiveBlit;          /* _PAD controller-warning blit */
extern struct pc_multiblit idle_warning_multiblit[1];   /* its multiblit (pc_prelude.h) */
extern OSMesgQueue *frame_dl;
extern s32 gGameState;                /* gGameState */
extern s32 gNewGameState;                /* gNewGameState */
typedef struct Clock { u8 pad0[636]; u32 tick; } Clock;
extern volatile Clock gScheduler;      /* scheduler clock; .tick = GVSyncCount */
extern s32 gLastKeyTime;
extern f32 gRetraceTime;
extern volatile s16 gRenderInProgress;
extern intptr_t blit_dl;
extern OSMesgQueue *frame_dl_start;
extern s32 gPauseState;                  /* gPauseState */

void *NewMultiBlit(s16, s16, void *, s32);
void RemoveBlit(void *);
void UpdateFrame(void);
void StartGraphics(void);
void UpdateRushControls(void);
void updateGameState(void);
void updateFrontEnd(void);
void updatePauseScreen(void);
void UpdateGame(void);
void DrawNoControllers(void);
void DrawBlits(void);
s32 UpdateEnvirons(void);
void UpdateCameras(void);
void ZOID_EndFrame(s32);

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
        frame_dl_start = frame_dl;
        UpdateRushControls();
    }
    if ((elapsed = (f32)(u32)(gScheduler.tick - gLastKeyTime) * gRetraceTime) > 300.0f && !(gGameState & 0x4000000F)) {
        if (!inactiveBlit) {
            inactiveBlit = NewMultiBlit(0, 0, idle_warning_multiblit, 1);
        }
    } else if (inactiveBlit) {
        RemoveBlit(inactiveBlit);
        inactiveBlit = 0;
    }
    updateGameState();
    if (gGameState & 0x7C03FFFE) {
        updateFrontEnd();
    }
    if ((gGameState & 0x200000) || (gGameState & 0x400000)) {
        updatePauseScreen();
        if (gPauseState) {
            DrawBlits();
            UpdateEnvirons();
            UpdateCameras();
            ZOID_EndFrame(0);
            return 1;
        }
    }
    if (gGameState & 0x3FC0000) {
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
