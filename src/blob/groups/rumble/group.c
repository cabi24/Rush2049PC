/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (one file: si_lock, its deleted static si_lock_init, the deleted static si_unlock_inline and si_unlock must be defined here; see below) */
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

/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct {
    char pad0[6];
    s8 present;
    char pad7[5];
    PcPfs pfs[1];               /* the OSPfs the motor calls take */
    s32 error, FreeSpace;
    s8 motorOn;
    s8 request;
    volatile s8 level;
    s8 unk7F;
    volatile s16 phase;
    char pad82[0x282];
} Controller;

extern OSMesgQueue rumble_queue;   /* retrace queue of this thread */
extern OSMesg rumble_msg;
extern OSMesgQueue si_lock_queue;   /* one-slot queue used as the SI lock */
extern OSMesg si_lock_msg;
extern s32 gScheduler;           /* scheduler */
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

static void si_lock_init(void) {
    si_lock_inited = 1;
    osCreateMesgQueue(&si_lock_queue, &si_lock_msg, 1);
    osSendMesg(&si_lock_queue, 0, 0);
}

void si_lock(void) {
    OSMesg msg;

    if (!si_lock_inited) {
        si_lock_init();
    }
    osRecvMesg(&si_lock_queue, &msg, 1);
}


void rumble_thread_entry(void *arg) {
    OSScClient client;
    OSMesg msg;
    s32 i;
    s32 retried;
    s32 level;
    Controller *p;

    osCreateMesgQueue(&rumble_queue, &rumble_msg, 1);
    osScAddClient(&gScheduler, &client, &rumble_queue);
    for (;;) {
        osRecvMesg(&rumble_queue, &msg, 1);
        si_lock();
        for (i = 0; i < 4; i++) {
            switch (gPaks[i].present) {
            case 0:
skip:
                continue;
            }
            retried = 0;
            if (++gPaks[i].phase >= 10) {
                gPaks[i].phase = 0;
            }
            level = gPaks[i].request;
            if (level > 40) {
                level = 40;
            }
            if (level < gPaks[i].level) {
                level = gPaks[i].level - 1;
            }
            gPaks[i].level = level;
            if (gRumble == 0) goto off;
            if (level > 0) goto check;
off:
            if (gPaks[i].motorOn) {
                if (!gRumble) {
                    osMotorInit(&si_event_queue, gPaks[i].pfs, i);
                }
                while (1) {
                    if (__osMotorAccess(gPaks[i].pfs, 0) == 0) {
                        gPaks[i].motorOn = 0;
                        break;
                    }
                    if (retried) break;
                    if (osMotorInit(&si_event_queue, gPaks[i].pfs, i) != 0) break;
                    retried = 1;
                }
            }
            goto skip;
check:
            if (level < 10) goto check2;
on:
            if (!gPaks[i].motorOn) {
                while (1) {
                    if (__osMotorAccess(gPaks[i].pfs, 1) == 0) {
                        gPaks[i].motorOn = 1;
                        break;
                    }
                    if (retried) break;
                    if (osMotorInit(&si_event_queue, gPaks[i].pfs, i) != 0) break;
                    retried = 1;
                }
            }
            goto skip;
check2:
            if (rumble_duty_masks[level] & (1 << gPaks[i].phase)) goto on;
            goto off;
        }
        si_unlock();
    }
}

void si_unlock(void);
