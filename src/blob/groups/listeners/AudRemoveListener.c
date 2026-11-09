/* AudRemoveListener and StopListeners: the players' listeners given back at a race's end (MAT3 audiomgr.c and
 * audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* The player record's field this file reads (MAT3 tPlayer.listener): its listener's number in gListeners
 * (StartListeners), -1 for none. */
typedef struct PlayerRec {
    /* 0x00 */ s32 unk0[17];
    /* 0x44 */ s32 listener;
    /* 0x48 */ void *unk48;
} PlayerRec;

extern PlayerRec gPlayers[];
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
