/* AudPlaySequence: a timed sequence of effects (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* PC: IDO's va_list walks the o32 stack after the last named argument; a host ABI passes the variable arguments
 * in registers (docs/port/SYNTH.md). */
typedef __builtin_va_list va_list;
#define va_start(list, parmN) __builtin_va_start(list, parmN)
#define va_arg(list, mode) __builtin_va_arg(list, mode)
#define va_end(list) __builtin_va_end(list)

int osRecvMesg(void *, void *, int);
int osSendMesg(void *, void *, int);

/* Starts a sequence of up to four effects (the num variable arguments, each an effect id or -1 to skip) that
 * ProcessSequences plays one after another, each with subId, replace and priority as AudPlayEffect's. If the
 * first has not started within timeout seconds the sequence is dropped. Returns the sequence's handle. */
u32 AudPlaySequence(int priority, f32 timeout, int replace, int subId, int num, ...)
{
    tAudioSequence *seq;
    va_list args;
    int i;
    u32 handle;

    osRecvMesg(&audio_lock_queue, 0, 1);
    seq = NewSequence();
    seq->priority = priority;
    seq->startTime = GetAudioTime();
    seq->timeout = timeout;
    seq->replace = replace;
    seq->subId = subId;
    seq->stop = 0;
    seq->next = 0;
    va_start(args, num);
    for (i = 0; i < num; i++) {
        seq->ids[i] = va_arg(args, int);
    }
    for (; i < 4; i++) {
        seq->ids[i] = -1;
    }
    va_end(args);
    seq->sndTag = -1;
    /* MAT3's _ActivateSequence */
    InsertLLE(&gActiveSequences, seq, gActiveSequences.head);
    seq->active = 1;
    handle = seq->id;
    osSendMesg(&audio_lock_queue, 0, 0);
    return handle;
}
