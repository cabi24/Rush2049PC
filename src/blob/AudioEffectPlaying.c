/* AudioEffectPlaying: the sound server's answer to whether an effect instance still sounds (MAT3 audiosfx.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Whether the effect instance of handle id (AudioPlayEffect's) is playing: 1 while it waits for its voice, then
 * whether MusyX still has its voice (sound_check); 0 for -1 or a handle whose instance has been reused.
 * The instance is the handle's low byte. */
s32 AudioEffectPlaying(s32 id) {
    u8 slot;
    tSfxVoice *instance;
    if (id != -1) {
        slot = id;
        if (id == gSfxInstances[slot].index) {
            if (gSfxInstances[slot].started != 0) {
                instance = gSfxInstances + slot;
                return sound_check(instance->sound) != -1;
            }
            return 1;
        }
    }
    return 0;
}
