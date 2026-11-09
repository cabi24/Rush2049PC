/* AudioTimeout: the sound clock's timer test (MAT3 audiosysps2.c). */
#include "game/game.h"

/* Whether interval seconds have passed on the sound clock since startTime. When the clock has wrapped since
 * startTime (it reads less), the end time is taken as wrapped too. */
s32 AudioTimeout(f32 startTime, f32 interval) {
    f32 endTime = startTime + interval;
    if (gAudioSeconds < startTime) endTime -= AUDIO_CLOCK_WRAP_SECONDS;
    return endTime < gAudioSeconds;
}
