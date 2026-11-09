/* StopSound: stops a car's sound and empties its record (MAT3 audio.c's _StopSound). */
#include "game/game.h"

/* Stops sound's effect, through the emitter calls when it was started as a 3D sound (emitter nonzero:
 * AudStopEmitter) or as a plain effect (AudStopEffect), and empties the record (InitSoundState). */
void StopSound(tSoundState *sound, s32 emitter) {
    if (emitter != 0) {
        AudStopEmitter(sound->sound);
    } else {
        AudStopEffect(sound->sound);
    }
    InitSoundState(sound);
}
