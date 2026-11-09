/* audio_unlock: gives back the sound manager's lock (N64 only). */
#include "game/game.h"

s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);

/* Puts audio_lock_queue's message back, for the next audio_lock. */
void audio_unlock(void) {
    osSendMesg(&audio_lock_queue, NULL, 0);
}
