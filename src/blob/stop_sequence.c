/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * stop_sequence (0x800FE7A4; historical label): under the audio_lock_queue event-queue lock, look up
 * the timed event with handle h in the event table gSequences (slot = h & gTagMask, valid only if
 * the slot's handle still equals h; h == -1 is "none") and set its flag at +0x0E. Event layout as in
 * src/blob/AudPlaySequence.c (0x3C bytes, handle @8).
 *
 * The lookup is a deleted inlined helper: the caller-less `jr ra; nop` stub find_sequence right before
 * this function (same two-return shape as ConvertTag in src/blob/groups/aud_stop_effect). Written
 * static here so the standalone scorer inlines it; in the whole-program unit the non-static form named
 * find_sequence also scores EQUAL with `the whole-program unit score stop_sequence --internal find_sequence`.
 * The two separate `gSequences[h & gTagMask]` address computations (test vs return) are retail's.
 */
typedef float f32;
typedef unsigned char u8;
typedef unsigned int u32;
typedef int s32;

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

extern char audio_lock_queue[];
extern Event *gSequences;
extern u32 gTagMask;
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
