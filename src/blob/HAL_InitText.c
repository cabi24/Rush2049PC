/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/*
 * Initialize a one-message synchronization queue once, then reset a state byte.
 * N64-specific adaptation using libultra queues. No direct arcade equivalent is
 * established; the Rush The Rock reference checkout is unavailable here.
 *
 * The named initialized value is the actual byte written to the guard. Its
 * local lifetime preserves the native IDO O2 address/constant scheduling.
 * No extra operation, argument, helper, volatile access or stack pad is used.
 */
typedef signed char s8;
typedef int s32;
typedef void *OSMesg;
typedef struct {
    void *mtqueue;
    void *fullqueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;

void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern s8 text_lock_inited;
extern s8 gFontIndex;
extern OSMesgQueue text_lock_queue;
extern OSMesg text_lock_msg;

void HAL_InitText(void)
{
    const s8 initialized = 1;

    if (!text_lock_inited) {
        text_lock_inited = initialized;
        osCreateMesgQueue(&text_lock_queue, &text_lock_msg, 1);
        osSendMesg(&text_lock_queue, (OSMesg)0, 0);
    }
    gFontIndex = -1;
}
