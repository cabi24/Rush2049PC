/* AudEffectPlaying: whether a sound the game asked for is still on (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);

/* Whether the effect of fx tag handle tag is playing or still to play: 1 while it waits for a voice or for its
 * restart (pending, interrupted), the sound server's answer while it plays (AudioEffectPlaying), 0 once its tag is
 * gone. */
s32 AudEffectPlaying(s32 tag) {
    s32 playing; tFxSequence *fx_tag;
    osRecvMesg(&audio_lock_queue, NULL, 1);
    fx_tag = ConvertTag(tag);
    if (fx_tag != NULL) {
        if (fx_tag->state == kFxPending || fx_tag->state == kFxInterrupted) { playing = 1; }
        else if (fx_tag->state != kFxPlaying) { playing = 0; }
        else { playing = AudioEffectPlaying(fx_tag->sysId); }
    } else { playing = 0; }
    osSendMesg(&audio_lock_queue, NULL, 0);
    return playing;
}
