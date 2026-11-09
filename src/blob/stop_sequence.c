#include "game/game.h"

/* PC: a sequence (audiomgr_thread.c's anon_ll_id_3c) has host-sized links, so its fields follow from +0x10, 72 bytes
 * (docs/port/SYNTH.md). */
#define EVENT_LINK0 void *pad0;
#define EVENT_LINK1 void *pad4;
typedef struct Event {
    EVENT_LINK0
    EVENT_LINK1
    u32 handle;     /* 8 */
    u8 busy;        /* 12 */
    u8 active;      /* 13 */
    u8 flag14;      /* 14 */
    u8 pad15;
    f32 start;      /* 16 */
    f32 delay;      /* 20 */
    u8 flag24;      /* 24 */
    u8 pad25[3];
    int args[4];    /* 28 */
    int data;       /* 44 */
    int func;       /* 48 */
    u8 type;        /* 52 */
    u8 pad53[3];
    int link;       /* 56 */
} Event;

VIEW(char[], audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(Event *, gSequences);
#define gSequences gSequences_view

int osRecvMesg(void *, void *, int);
int osSendMesg(void *, void *, int);

static Event *find_sequence(s32 h)
{
    if (h == -1) {
        return 0;
    }
    if (gSequences[h & gTagMask].handle != h) {
        return 0;
    }
    return &gSequences[h & gTagMask];
}

void stop_sequence(s32 h)
{
    Event *e;

    osRecvMesg(audio_lock_queue, 0, 1);
    e = find_sequence(h);
    if (e != 0) {
        e->flag14 = 1;
    }
    osSendMesg(audio_lock_queue, 0, 0);
}
