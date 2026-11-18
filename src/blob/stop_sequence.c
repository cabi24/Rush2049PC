/* stop_sequence: stop a sequence of timed sound events (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

int osRecvMesg(void *, void *, int);
int osSendMesg(void *, void *, int);

/* The sequence of handle h (its slot is h's tag bits), or NULL for -1 or a handle the slot no longer holds. */
static tAudioSequence *find_sequence(s32 h)
{
    if (h == -1) {
        return 0;
    }
    if (gSequences[h & gTagMask].id != h) {
        return 0;
    }
    return &gSequences[h & gTagMask];
}

/* Asks the sequence of handle h, if it still runs, to stop (its stop flag), under the audio lock. */
void stop_sequence(s32 h)
{
    tAudioSequence *e;

    osRecvMesg(&audio_lock_queue, 0, 1);
    e = find_sequence(h);
    if (e != 0) {
        e->stop = 1;
    }
    osSendMesg(&audio_lock_queue, 0, 0);
}
