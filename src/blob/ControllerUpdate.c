#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;

typedef struct OSContPad {u16 button;s8 stick_x,stick_y;u8 errno;} OSContPad;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);

typedef struct {
    s8 connected;       /* 0x0 */
    u8 pad1[3];
    u32 buttons;        /* 0x4 */
    s8 stick[2];        /* 0x8 */
    s8 unkA[4];         /* 0xA */
    u8 padE[2];
} ContRec16;

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view

VIEW(volatile ContRec16[4], gControllerData);
#define gControllerData gControllerData_view
VIEW(u32[19], n64_button_map);
#define n64_button_map n64_button_map_view

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
