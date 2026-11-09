#include "game/game.h"

/* PC: IDO's va_list walks the o32 stack after the last named argument; a host ABI passes the variable arguments
 * in registers (docs/port/SYNTH.md). */
typedef __builtin_va_list va_list;
#define va_start(list, parmN) __builtin_va_start(list, parmN)
#define va_arg(list, mode) __builtin_va_arg(list, mode)
#define va_end(list) __builtin_va_end(list)

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

typedef struct EventList {
    u8 indirect, doubly, pad2[2];
    u32 count;
    Event *head, *tail;
} EventList;

VIEW(char[], audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(EventList, gActiveSequences);
#define gActiveSequences gActiveSequences_view

int osRecvMesg(void *, void *, int);
int osSendMesg(void *, void *, int);
Event *NewSequence(void);

u32 AudPlaySequence(int type, f32 delay, int func, int data, int nargs, ...)
{
    Event *e;
    va_list ap;
    int i;
    u32 handle;

    osRecvMesg(audio_lock_queue, 0, 1);
    e = NewSequence();
    e->type = type;
    e->start = GetAudioTime();
    e->delay = delay;
    e->func = func;
    e->data = data;
    e->flag14 = 0;
    e->flag24 = 0;
    va_start(ap, nargs);
    for (i = 0; i < nargs; i++) {
        e->args[i] = va_arg(ap, int);
    }
    for (; i < 4; i++) {
        e->args[i] = -1;
    }
    va_end(ap);
    e->link = -1;
    InsertLLE(&gActiveSequences, e, gActiveSequences.head);
    e->active = 1;
    handle = e->handle;
    osSendMesg(audio_lock_queue, 0, 0);
    return handle;
}
