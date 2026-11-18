/* AudEffectPlaying: whether a sound the game asked for is still on (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"


/* Whether the effect of fx tag handle tag is playing or still to play: 1 while it waits for a voice or for its
 * restart (pending, interrupted), the sound server's answer while it plays (AudioEffectPlaying), 0 once its tag is
 * gone. */
s32 AudEffectPlaying(s32 tag) {
    s32 playing; tFxSequence *fx_tag;
    lock_take(&audio_lock_queue);
    fx_tag = ConvertTag(tag);
    if (fx_tag != NULL) {
        if (fx_tag->state == kFxPending || fx_tag->state == kFxInterrupted) { playing = 1; }
        else if (fx_tag->state != kFxPlaying) { playing = 0; }
        else { playing = AudioEffectPlaying(fx_tag->sysId); }
    } else { playing = 0; }
    lock_give(&audio_lock_queue);
    return playing;
}
