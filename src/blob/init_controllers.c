#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;

typedef struct Rumble16 {u8 flag;u8 pad[15];} Rumble16;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);
extern void osSetEventMesg(s32,OSMesgQueue *,OSMesg);  /* SDK osSetEventMesg */

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view
VIEW(OSMesg[8], si_event_msgs);
#define si_event_msgs si_event_msgs_view

VIEW(Rumble16[4], gControllerData);
#define gControllerData gControllerData_view

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
