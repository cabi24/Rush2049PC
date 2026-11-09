/* PS_ErrorFunc: the pak system's error function, the error screen in the middle of whatever was running (MAT3
 * pakerr.c). */
#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;

VIEW(struct pc_multiblit[1], PS_ErrorFunc_dimMultiBlit);
#define PS_ErrorFunc_dimMultiBlit PS_ErrorFunc_dimMultiBlit_view

extern OSId osGetThreadId(OSThread *thread);

void ZOID_SetFog(s16 fogNear, s16 fogFar, u8 r, u8 g, u8 b, u8 a, s32 window);

#define GAME_THREAD_ID 7   /* the game thread's id: only it may draw frames */

/* Shows pak's error mode (kPS_Err*) and waits for the player's answer: the screen is dimmed (a full-screen blit and
 * every window's fog pushed to black), the error dialog made (SetPakError) and run until done (ShowPakError, which
 * writes the answer to *retry and *status and closes by itself when poll says the pak is gone), drawing frames when
 * called on the game thread and serving the paks unless the error is about reading the pak itself (10 to 13: the
 * pak server is what called). Then the fog and the screen are put back. The pak system calls it as gErrorFunc;
 * blocks is unused. */
void PS_ErrorFunc(s32 pak, s32 mode, s32 blocks, s32 unused, s8 *retry, s8 *status, s32 (*poll)(s32)) {
    s8 done;
    tBlit *dim;
    s16 fog_near[4];
    s16 fog_far[4];
    u8 fog_r[4];
    u8 fog_g[4];
    u8 fog_b[4];
    u8 fog_a[4];
    s32 i;

    done = 0;
    dim = NewMultiBlit(0, 0, &PS_ErrorFunc_dimMultiBlit, 1);
    SetPakError(pak, mode, 10, 10, 300, 180);
    for (i = 0; i < gNumWindows; i++) {
        ZOID_GetFog(&fog_near[i], &fog_far[i], &fog_r[i], &fog_g[i], &fog_b[i], &fog_a[i], i);
        ZOID_SetFog(1000, 999, 0, 0, 0, 255, i);
    }
    while (!done) {
        if (osGetThreadId(0) == GAME_THREAD_ID) {
            UpdateFrame();
        }
        if (mode != kPS_ErrReread && mode != kPS_ErrRepairFailed && mode != kPS_ErrRepaired && mode != kPS_ErrRecognize) {
            SaveServer(0, 0);
        }
        ShowPakError(pak, retry, status, &done, poll);
    }
    RemoveBlit(dim);
    for (i = 0; i < gNumWindows; i++) {
        ZOID_SetFog(fog_near[i], fog_far[i], fog_r[i], fog_g[i], fog_b[i], fog_a[i], i);
    }
}
