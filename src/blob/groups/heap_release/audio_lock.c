/* audio_lock: takes the sound manager's lock (N64 only: MAT3's sound manager runs on the game's thread). */
#include "game/game.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);

/* Waits for audio_lock_queue's one message: the lock that the game's requests and the audio manager thread take
 * around the sound manager's lists and tags. audio_unlock gives it back. */
void audio_lock(void) {
    osRecvMesg(&audio_lock_queue, NULL, 1);
}
