/* update_frame_on_game_thread: ends the frame, if called on the game thread. Nothing in the game calls it. */
#include "game/game.h"

/* Ends the frame (UpdateFrame) when the calling thread is the game thread. Neither argument matters; the empty test
 * is the N64 code's. */
void update_frame_on_game_thread(s32 unused, s32 passed)
{
    if (unused) {}
    if (thread_id(NULL) == THREAD_ID_GAME) {
        UpdateFrame();
    }
}
