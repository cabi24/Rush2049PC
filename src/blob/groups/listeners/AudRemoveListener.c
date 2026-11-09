#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct Link { struct Link *next, *prev; } Link;
typedef struct List { u8 indirect, doubly, pad[2]; u32 count; Link *head, *tail; } List;
typedef struct OSMesgQueue OSMesgQueue;

typedef struct Entry {
    /* 0x00 */ s32 unk0[17];
    /* 0x44 */ s32 object;
    /* 0x48 */ void *unk48;
} Entry;
/* PC: a packet's type (N64 +8) and ready (+9) bytes follow its Link, two host pointers here (AudAddListener's
 * Packet, AudInit3D's RecA), so they are at host +16 and +17. At the N64 offsets AudRemoveListener read and
 * wrote the low bytes of the link's prev pointer: quitting a two-player battle unlinked through it and faulted
 * (docs/port/QA.md 5). */
#define PKT(n) (2 * (s32)sizeof(void *) + (n) - 8)
/* PC: an input record is 80 bytes (its pointer at +0x48 is 8 bytes); +0x44 stays 32 bits and holds a packet's
 * window address (the packets are a heap block, AudAddListener) or -1, which the node pointer zero-extends from
 * and -1 keeps (docs/port/POINTERS.md 10). */
#define OBJECT(x) ((x) == -1 ? (s32 *)-1 : (s32 *)PC_N64P((u32)(x)))

VIEW(OSMesgQueue, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(List, gFreeListeners);
#define gFreeListeners gFreeListeners_view
VIEW(List, gActiveListeners);
#define gActiveListeners gActiveListeners_view

extern Entry gPlayers[];
s32 osRecvMesg(OSMesgQueue *mq, void *msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, void *msg, s32 flag);

__inline void AudRemoveListener(s32 *arg0)
{
    if (arg0 == (s32 *)-1) {
        return;
    }
    osRecvMesg(&audio_lock_queue, 0, 1);
    ValidateListener(arg0);
    if (((s8 *)arg0)[PKT(9)] != 0) {
        RemoveLLE(&gActiveListeners, arg0);
        ((s8 *)arg0)[PKT(9)] = 0;
    }
    InsertLLE(&gFreeListeners, arg0, gFreeListeners.head);
    ((s8 *)arg0)[PKT(8)] = 1;
    osSendMesg(&audio_lock_queue, 0, 0);
}

void StopListeners(void)
{
    s32 i;

    for (i = 0; i < gNumPlayers; i++) {
        AudRemoveListener(OBJECT(gPlayers[i].object));
        gPlayers[i].object = -1;
    }
}
