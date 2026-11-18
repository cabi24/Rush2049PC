/* musyx_set_retrace_callback: the MusyX sound library's service that installs its retrace callback (one of its
 * eight services, docs/code/r5.md "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

/* Make callback the function the audio retrace thread calls each retrace (audio_retrace_thread_entry). Always 0,
 * success. */
s32 musyx_set_retrace_callback(void (*callback)(void)) {
    musyx_retrace_callback = callback;
    return 0;
}
