/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (one file: audio_prenmi is inlined here; the audio_queue_process.c bodies are in the file only for the `.align 5` position, see below) */
/*
 * Historical label audio_retrace_thread_entry is misleading: this is a scheduler-client
 * thread entry (N64-only, no arcade ancestor).  It registers a client on the
 * scheduler gScheduler with its own 8-slot queue audio_retrace_queue and loops forever
 * on the queue.  Message type 1 (retrace): if the callback musyx_retrace_callback is set,
 * spin while the scheduler's current task (+0x274) is scheduler_audio_task, then call
 * it; then advance the clock gAudioSeconds by gRetraceTime and wrap it at 14400.
 * Message type 4 (pre-NMI): audio_prenmi(), i.e. sound_freeze_output().
 *
 * Whole-program context this body needs:
 *   - audio_prenmi (0x8008AA20, the function in front of it) is
 *     inlined by umerge for `case 4`; the inlined call is the 8 frame bytes
 *     that a direct sound_freeze_output() call leaves missing (96 instead of 104).
 *   - the unreachable epilogue after the endless loop is `.align 5`, so the
 *     nop count depends on this function's offset in the object.  Retail has
 *     it 0x10 past a 32-byte boundary (the whole-program object starts at
 *     0x80086A50).  Here the real neighbours si_unlock,
 *     si_lock and rumble_thread_entry (0x350 bytes, bodies unchanged
 *) and the thunk (0x20) are
 *     emitted in front of it: the definitions of this file's two functions
 *     come first in the source because umerge emits uncalled roots in
 * reverse definition order. In the whole-program unit the
 *     body is 103 words instead of 105 for the same reason (two nops) and
 *     needs an `align` entry in src/blob/unit_overrides.json, like
 *     rumble_thread_entry and sample_dma_thread_entry.
 *
 * Shaping quirks (match the bytes, may not be the original spelling):
 *   - the callback is called with itself as argument, `musyx_retrace_callback(musyx_retrace_callback)`:
 *     retail keeps the pointer in a0 (`lw a0; beqzl a0; ...; jalr a0`); a
 *     plain `musyx_retrace_callback()` colours it v1 (4 words).  INFERRED: the real
 *     callback may take some other value that IDO folded onto the same web.
 *   - `void *volatile cur` on the scheduler field +0x274 (retail reloads it in
 *     the spin loop while scheduler_audio_task stays in v0); operand order
 *     `scheduler_audio_task == gScheduler.cur`.
 *   - the scheduler extern is typed here (struct Sched) instead of the `s32`
 *     audio_queue_process.c uses; that file only takes its address.
 */
typedef float f32;
/*
 * Historical label rumble_thread_entry is misleading: this is the Rumble Pak
 * thread entry (N64-only, no arcade ancestor).  It registers a scheduler
 * client, and on every retrace message walks the four 0x304-byte controller
 * records at gPaks under the SI lock (queue si_lock_queue):
 *   - phase (+0x80) counts 0..9;
 *   - current level (+0x7E) follows the requested level (+0x7D, capped at 40)
 *     upward at once and downward by one per frame;
 *   - the motor is on when rumble is enabled (gRumble), level > 0 and
 *     either level >= 10 or bit `phase` of the duty table rumble_duty_masks[level]
 *     is set (PWM for weak levels); otherwise off;
 *   - motor on/off goes through 0x80009F20(pfs, on) (label osMotorInit: really
 *     the motor access call) and, on failure, one re-init through
 *     0x8000A194(siQueue, pfs, port) (label osMotorStart: really the pak init)
 *     followed by a single retry.
 *
 * Whole-program context this body needs (all are real neighbours):
 *   - si_lock (0x8008A704, the lazy-initialising lock) is inlined here by
 *     umerge.  It only reproduces (and itself scores MATCH from this file)
 *     when its one-time initialisation is a separate static, inlined and
 *     deleted: that is the `jr ra; nop` stub si_lock_init in front of it.
 *   - the stub si_unlock_inline between the lock and this function is a second
 *     deleted static.  Each inlined call costs 8 bytes of this frame, and the
 *     frame has room for exactly three (lock, lock init, one more), so the
 *     unlock at the bottom is that static, with a direct osJamMesg.  Its body
 *     is INFERRED (calling the matched si_unlock instead gives the
 *     same code with a frame 8 bytes too large).
 *   - the epilogue after the endless loop is `.align 5`: the nop count
 *     depends on this function's offset in the object.  Retail has
 *     si_unlock, stub, lock, stub in front of it (0xAC bytes from a
 *     32-byte boundary); si_unlock is defined last here because
 *     umerge emits uncalled roots in reverse definition order, which puts it
 *     first in the object.
 *
 * Shaping quirks (match the bytes, may not be the original spelling):
 *   - `volatile` on fields +0x7E and +0x80 (retail reloads them);
 *   - `switch (present) { case 0: skip: continue; }` with `goto skip` exits:
 *     retail keeps an empty block after the presence test that every exit of
 *     the body joins (bnezl/b pair); a plain `if (...) continue;` folds it;
 *   - goto layout of the on/off decision (off block first, duty-table test
 *     last), matching retail block order;
 *   - unused local `p` supplies 4 bytes of the frame; operand order
 *     `table & (1 << phase)`.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

typedef struct { s32 w[6]; } OSMesgQueue;
typedef void *OSMesg;
typedef struct { void *next; OSMesgQueue *msgQ; } OSScClient;

typedef struct {
    /* 0x000 */ char pad0[6];
    /* 0x006 */ s8 present;
    /* 0x007 */ char pad7[5];
    /* 0x00C */ char pfs[0x70];
    /* 0x07C */ s8 motorOn;
    /* 0x07D */ s8 request;
    /* 0x07E */ volatile s8 level;
    /* 0x07F */ s8 unk7F;
    /* 0x080 */ volatile s16 phase;
    /* 0x082 */ char pad82[0x282];
} Controller; /* 0x304 */

extern OSMesgQueue rumble_queue;   /* retrace queue of this thread */
extern OSMesg rumble_msg;
extern OSMesgQueue si_lock_queue;   /* one-slot queue used as the SI lock */
extern OSMesg si_lock_msg;
typedef struct {
    char pad[0x2B0];   /* gScheduler's curRSPTask (scheduler_current_rsp_task) natively: docs/port/ULTRA.md */
    void *volatile cur;
} Sched;
extern Sched gScheduler;         /* scheduler */
extern OSMesgQueue si_event_queue;   /* SI message queue */
extern s8 si_lock_inited;            /* lock initialised */
extern s8 gRumble;            /* rumble enabled */
extern u16 rumble_duty_masks[];         /* duty masks for levels 0..9 */
extern Controller gPaks[4];

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void osScAddClient(void *sc, OSScClient *c, OSMesgQueue *mq);
s32 __osMotorAccess(void *pfs, s32 on);
s32 osMotorInit(OSMesgQueue *mq, void *pfs, s32 port);
void si_unlock(void);


/* ---- this file's claim: audio_prenmi (context, already locked) and audio_retrace_thread_entry ---- */
extern OSMesgQueue audio_retrace_queue;   /* this thread's queue */
extern OSMesg audio_retrace_msgs[8];
extern f32 gAudioSeconds;           /* accumulator, wraps at 14400 */
extern void (*musyx_retrace_callback)();     /* per-retrace callback, may be null */
extern void *scheduler_audio_task;         /* task the callback must not race */
extern f32 gRetraceTime;           /* increment per retrace */

void sound_freeze_output(void);

void audio_prenmi(void);


void audio_retrace_thread_entry(void *arg) {
    OSScClient client;
    s16 *msg = 0;

    osCreateMesgQueue(&audio_retrace_queue, audio_retrace_msgs, 8);
    osScAddClient(&gScheduler, &client, &audio_retrace_queue);
    gAudioSeconds = 0.0f;
    while (1) {
        osRecvMesg(&audio_retrace_queue, (OSMesg *) &msg, 1);
        switch (*msg) {
        case 1:
            if (musyx_retrace_callback != 0) {
                while (scheduler_audio_task != 0 && scheduler_audio_task == gScheduler.cur) { pc_os_spin_yield("audio_retrace_thread_entry");
                }
                musyx_retrace_callback(musyx_retrace_callback);
            }
            gAudioSeconds += gRetraceTime;
            if (gAudioSeconds > 14400.0f) {
                gAudioSeconds -= 14400.0f;
            }
            break;
        case 4:
            audio_prenmi();
            break;
        }
    }
}



void si_unlock(void);

