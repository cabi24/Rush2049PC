/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* CONTEXT (byte-identical body of the locked rumble_thread member si_lock = the public pak/SI lock and
 * its deleted static init) so the group compile inlines it into PS_PakServer as the unit does. Not claimed. */
typedef signed char s8;
typedef signed int s32;
typedef void *OSMesg;
typedef struct OSMesgQueue OSMesgQueue;
extern OSMesgQueue si_lock_queue;   /* one-slot queue used as the SI lock */
extern OSMesg si_lock_msg;
extern s8 si_lock_inited;            /* lock initialised */
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

static void si_lock_init(void) {
    si_lock_inited = 1;
    osCreateMesgQueue(&si_lock_queue, &si_lock_msg, 1);
    osSendMesg(&si_lock_queue, 0, 0);
}

