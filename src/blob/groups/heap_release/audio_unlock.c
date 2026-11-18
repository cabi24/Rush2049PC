/* audio_unlock: gives back the sound manager's lock (N64 only). */
#include "game/game.h"


/* Puts audio_lock_queue's message back, for the next audio_lock. */
void audio_unlock(void) {
    lock_give(&audio_lock_queue);
}
