/* AudioTimeoutPerc: how far a sound-clock timer has run (MAT3 audiosysps2.c). */
#include "game/game.h"

/* The sound clock gAudioSeconds wraps every four hours (audio_retrace_thread_entry). */
#define AUDIO_CLOCK_WRAP_SECONDS 14400.0f

/* The fraction of interval seconds that has passed since startTime on the sound clock (above 1 once it has run
 * out), allowing for one wrap of the clock. */
f32 AudioTimeoutPerc(f32 startTime, f32 interval) {
    f32 now;

    now = gAudioSeconds;
    if (gAudioSeconds < startTime) {
        now = gAudioSeconds + AUDIO_CLOCK_WRAP_SECONDS;
    }
    return (now - startTime) / interval;
}
