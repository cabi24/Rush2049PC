/* update_frame_on_game_thread: ends the frame, if called on the game thread. Nothing in the game calls it. */
#include "game/game.h"

/* osGetThreadId as the N64 code calls it, with a second argument it doesn't read. */
s32 osGetThreadId(void *thread, s32 unused);

/* Ends the frame (UpdateFrame) when the calling thread is the game thread (thread 7). Neither argument matters; the
 * empty test is the N64 code's. */
void update_frame_on_game_thread(s32 unused, s32 passed)
{
    if (unused) {}
    if (osGetThreadId(0, passed) == 7) {
        UpdateFrame();
    }
}
