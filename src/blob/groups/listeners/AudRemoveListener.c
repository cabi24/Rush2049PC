/* AudRemoveListener and StopListeners: the players' listeners given back at a race's end (MAT3 audiomgr.c and
 * audio.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* The player record's field this file reads (MAT3 tPlayer.listener). */
typedef struct PlayerRec {
    /* 0x00 */ s32 unk0[17];
    /* 0x44 */ s32 listener;
    /* 0x48 */ void *unk48;
} PlayerRec;
/* PC: a player record is 80 bytes (its pointer at +0x48 is 8 bytes); +0x44 stays 32 bits and holds a listener's
 * window address (the listeners are a heap block, AudAddListener) or -1, which the pointer zero-extends from
 * and -1 keeps (docs/port/POINTERS.md 10). At the N64's offsets (free +8, active +9) AudRemoveListener read and
 * wrote the low bytes of the links' prev pointer: quitting a two-player battle unlinked through it and faulted
 * (docs/port/QA.md 5); the fields are tSfxListener's now. */
#define LISTENER(x) ((x) == -1 ? (tSfxListener *)-1 : (tSfxListener *)PC_N64P((u32)(x)))

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
        AudRemoveListener(LISTENER(gPlayers[i].listener));
        gPlayers[i].listener = -1;
    }
}
