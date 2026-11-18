/* AudRemoveListener and StopListeners: the players' listeners given back at a race's end (MAT3 audiomgr.c and
 * audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"


/* Gives listener back (MAT3's _FreeListener, inlined): off the active list, onto the free list. -1 (none) is
 * ignored. */
__inline void AudRemoveListener(tSfxListener *listener)
{
    if (listener == (tSfxListener *)-1) {
        return;
    }
    lock_take(&audio_lock_queue);
    ValidateListener(listener);
    if (listener->active != 0) {
        RemoveLLE(&gActiveListeners, listener);
        listener->active = 0;
    }
    InsertLLE(&gFreeListeners, listener, gFreeListeners.head);
    listener->free = 1;
    lock_give(&audio_lock_queue);
}

/* Gives back every player's listener (StartListeners') and forgets it. */
void StopListeners(void)
{
    s32 i;

    for (i = 0; i < gNumPlayers; i++) {
        AudRemoveListener(gPlayers[i].listener == -1 ? (tSfxListener *)-1 : &gListeners[gPlayers[i].listener]);
        gPlayers[i].listener = -1;
    }
}
