/* AudRemoveListener and StopListeners: the players' listeners given back at a race's end (MAT3 audiomgr.c and
 * audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);

/* Gives listener back (MAT3's _FreeListener, inlined): off the active list, onto the free list. -1 (none) is
 * ignored. */
__inline void AudRemoveListener(tSfxListener *listener)
{
    if (listener == (tSfxListener *)-1) {
        return;
    }
    osRecvMesg(&audio_lock_queue, 0, 1);
    ValidateListener(listener);
    if (listener->active != 0) {
        RemoveLLE(&gActiveListeners, listener);
        listener->active = 0;
    }
    InsertLLE(&gFreeListeners, listener, gFreeListeners.head);
    listener->free = 1;
    osSendMesg(&audio_lock_queue, 0, 0);
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
