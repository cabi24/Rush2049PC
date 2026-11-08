/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* audio_unlock @ 0x80091AF8 (locked caller-less 2-word stub, given its body): slot-queue unlock: osJamMesg(&audio_lock_queue, NULL, OS_MESG_NOBLOCK).
 * Internal and umerge-inlined into every caller (AudioShutdown here); only the retail jr-ra stub remains,
 * like the heap lock/unlock stubs heap_lock/heap_unlock in audio_sfx_shutdown.c. */
typedef int s32;
typedef struct OSMesgQueue OSMesgQueue;
typedef void *OSMesg;
#define NULL ((void *)0)
extern s32 audio_lock_queue;
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

void audio_unlock(void) {
    osSendMesg((OSMesgQueue *) &audio_lock_queue, NULL, 0);
}


