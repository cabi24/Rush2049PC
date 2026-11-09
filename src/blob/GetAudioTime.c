/* GetAudioTime: the sound clock (MAT3 audiosysps2.c). */
#include "game/game.h"

/* The sound clock in seconds: gAudioSeconds, which the audio retrace thread advances and wraps every four hours. */
f32 GetAudioTime(void) {
    return gAudioSeconds;
}
