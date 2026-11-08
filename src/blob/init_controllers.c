/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * init_controllers (historical label): controller subsystem init. Creates the SI event queue si_event_queue
 * (8-entry buffer si_event_msgs), registers it for OS_EVENT_SI (5) with a message pointing at a static s16
 * holding 5 (osSetEventMesg, 0x80006E10, historical label osSetEventMesgAlt), then under the SI/pak lock
 * (queue si_lock_queue, created once with si_lock_inited as the init flag; same helpers as the matched
 * pak_file_unchanged / PS_RumbleStop) calls osContInit(&si_event_queue, &cont_bit_pattern [bit pattern],
 * cont_status [OSContStatus[]]) (0x80009450, historical label __osContBuildPacket), releases the lock and
 * clears the first byte of the four 16-byte rumble records gControllerData. N64-only, no arcade ancestor.
 *
 * Shaping: the message is a FUNCTION-LOCAL static (own.bss at 0x80150F58, nothing else in the image
 * references that address). As an extern, uopt CSEs the store address with the argument address into one
 * register (`la a2; sh t6,0(a2)`); as a local static ugen keeps `sh t6,%lo(sym)(at)` and builds the
 * argument separately, as retail. (An extern struct at si_event_msgs with the message at +0x40 also gives
 * identical code but no such struct exists: gBBoxModelStructs/F40 belong to other code.)
 */
typedef unsigned char u8;typedef signed char s8;typedef unsigned short u16;typedef short s16;
typedef unsigned int u32;typedef int s32;
typedef void *OSMesg;
typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;
typedef struct OSContStatus {u16 type;u8 status,errno;} OSContStatus;
typedef struct Rumble16 {u8 flag;u8 pad[15];} Rumble16;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);
extern void osSetEventMesg(s32,OSMesgQueue *,OSMesg);  /* SDK osSetEventMesg */
extern s32 osContInit(OSMesgQueue *,u8 *,OSContStatus *);  /* SDK osContInit */
extern s8 si_lock_inited;
extern OSMesgQueue si_lock_queue;
extern OSMesg si_lock_msg;
extern OSMesgQueue si_event_queue;
extern OSMesg si_event_msgs[8];
extern u8 cont_bit_pattern;
extern OSContStatus cont_status[];
extern Rumble16 gControllerData[4];

static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue,&si_lock_msg,1);
        osSendMesg(&si_lock_queue,0,0);
    }
}

static void pak_lock(void)
{
    OSMesg message;

    pak_queue_init();
    osRecvMesg(&si_lock_queue,&message,1);
}

void si_unlock(void);

void init_controllers(void)
{
    static s16 si_message;  /* 0x80150F58, only referenced here */
    s32 i;

    osCreateMesgQueue(&si_event_queue, si_event_msgs, 8);
    si_message = 5;
    osSetEventMesg(5, &si_event_queue, &si_message);
    pak_lock();
    osContInit(&si_event_queue, &cont_bit_pattern, cont_status);
    si_unlock();
    for (i = 0; i < 4; i++) {
        gControllerData[i].flag = 0;
    }
}
