/* musyx_set_retrace_callback: the MusyX sound library's service that installs its retrace callback (one of its
 * eight services, docs/code/r5.md "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

VIEW(intptr_t, musyx_retrace_callback);
#define musyx_retrace_callback musyx_retrace_callback_view

/* Make callback the function the audio retrace thread calls each retrace (audio_retrace_thread_entry). Always 0,
 * success. */
s32 musyx_set_retrace_callback(intptr_t callback) {
    musyx_retrace_callback = callback;
    return 0;
}
