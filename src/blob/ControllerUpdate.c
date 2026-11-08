/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* func_800E73D8 (0x800E73D8, 596 B): N64 per-frame controller read (model thread; no MAT3/arcade ancestor).
 * Under the SI/pak lock (pak_queue_init/pak_lock/pak_unlock statics, as in the matched init_controllers and
 * pak_file_unchanged): drain the SI event queue si_event_queue; every 30th call (function-local static counter, retail
 * .data byte 0x1D = 29 so the first call queries) re-query the pads (osContStartQuery, wait for SI message 5,
 * osContGetQuery(cont_status), pak_poll_inserted()); start a read (osContStartReadData; on failure unlock and return 0),
 * wait, osContGetReadData(pads), unlock; per controller: connected flag from the status/pad errno bit 8
 * (CONT_NO_RESPONSE_ERROR), disconnected pads read as zero, 19-entry button map n64_button_map -> game button mask,
 * stick copied, record bytes 10..13 cleared; then ControlIRQ() and return 1.
 * Base: best (4 words: the counter web in v1, retail v0). : the compiled-out block end
 * `do {} while (0);` (disclosed; emits no code) after the drain splits uopt's basic block between the
 * osRecvMesg call and the counter, so v0 (the call's precoloured return) no longer forbids v0 for the counter web.
 */
typedef unsigned char u8;typedef signed char s8;typedef unsigned short u16;typedef short s16;
typedef unsigned int u32;typedef int s32;
typedef void *OSMesg;
typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;
typedef struct OSContStatus {u16 type;u8 status,errno;} OSContStatus;
typedef struct OSContPad {u16 button;s8 stick_x,stick_y;u8 errno;} OSContPad;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);
extern s32 osContStartQuery(OSMesgQueue *);
extern void osContGetQuery(OSContStatus *);
extern s32 osContStartReadData(OSMesgQueue *);
extern void osContGetReadData(OSContPad *);
typedef struct {
    s8 connected;       /* 0x0 */
    u8 pad1[3];
    u32 buttons;        /* 0x4 */
    s8 stick[2];        /* 0x8 */
    s8 unkA[4];         /* 0xA */
    u8 padE[2];
} ContRec16;
extern s8 si_lock_inited;
extern OSMesgQueue si_lock_queue;
extern OSMesg si_lock_msg;
extern OSMesgQueue si_event_queue;
extern OSContStatus cont_status[];
extern volatile ContRec16 gControllerData[4];
extern u32 n64_button_map[19];
void pak_poll_inserted(void);
void ControlIRQ(void);

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

s32 ControllerUpdate(void)
{
    static u8 queryTimer = 29;
    OSMesg msg = 0;
    s32 i;
    s32 j;
    OSContPad pads[4];

    pak_lock();
    osRecvMesg(&si_event_queue, &msg, 0);
    do {} while (0);
    if (++queryTimer == 30) {
        queryTimer = 0;
        osContStartQuery(&si_event_queue);
        do {
            osRecvMesg(&si_event_queue, &msg, 1);
        } while (*(s16 *)msg != 5);
        osContGetQuery(cont_status);
        pak_poll_inserted();
    }
    if (osContStartReadData(&si_event_queue) != 0) {
        si_unlock();
        return 0;
    }
    osRecvMesg(&si_event_queue, &msg, 1);
    osContGetReadData(pads);
    si_unlock();
    for (i = 0; i < 4; i++) {
        if ((cont_status[i].errno & 8) || (pads[i].errno & 8)) {
            if (gControllerData[i].connected) {
                gControllerData[i].connected = 0;
            }
        } else {
            if (!gControllerData[i].connected) {
                gControllerData[i].connected = 1;
            }
        }
        if (!gControllerData[i].connected) {
            pads[i].stick_x = pads[i].stick_y = 0;
            pads[i].button = 0;
        }
        gControllerData[i].buttons = 0;
        for (j = 0; j < 19; j++) {
            if (pads[i].button & n64_button_map[j]) {
                gControllerData[i].buttons |= 1 << j;
            }
        }
        gControllerData[i].stick[0] = pads[i].stick_x;
        gControllerData[i].stick[1] = pads[i].stick_y;
        gControllerData[i].unkA[0] = 0;
        gControllerData[i].unkA[1] = 0;
        gControllerData[i].unkA[2] = 0;
        gControllerData[i].unkA[3] = 0;
    }
    ControlIRQ();
    return 1;
}
