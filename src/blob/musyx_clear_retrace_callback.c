/* musyx_clear_retrace_callback: the MusyX sound library's service that stops its retrace callback (one of its eight
 * services, docs/code/r5.md "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

VIEW(intptr_t, musyx_retrace_callback);
#define musyx_retrace_callback musyx_retrace_callback_view

/* Clear the function the audio retrace thread calls each retrace (audio_retrace_thread_entry). The word the library
 * passes (the callback it set) is not used. */
void musyx_clear_retrace_callback(s32 callback) {
    musyx_retrace_callback = 0;
}
