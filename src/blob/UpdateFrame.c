/* UpdateFrame: the end of a frame on the game thread (MAT3 game.c's frame end, the N64's scheduler hand-off). */
#include "game/game.h"

/* Asks the main thread to end the frame (GAME_MSG_END_FRAME) and waits on the game thread's queue for the next
 * frame's start, emptying it of any older messages; then the new frame's display list, the frame timer, the controls
 * and the once-a-frame functions. */
void UpdateFrame(void)
{
    OSScMsg sm;
    OSMesg msg;

    frame_end_requested = 1;
    sm.type = GAME_MSG_END_FRAME;
    queue_send(&main_thread_queue, &sm);
    game_frame_building = 0;
    queue_try_recv(&game_thread_queue, &msg);
    msg = queue_recv(&game_thread_queue);
    while (queue_try_recv(&game_thread_queue, &msg)) {
    }
    game_frame_building = 1;
    frame_start_pending = 0;
    frame_dl_start = frame_dl;
    UpdateFrameTimer();
    UpdateRushControls();
    HAL_CallOncePerFrameFuncs();
}
