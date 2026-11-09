#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue { u8 opaque[0x18]; } OSMesgQueue;
typedef struct OSScClient { struct OSScClient *next; OSMesgQueue *msgQ; } OSScClient;
typedef struct OSScMsg { s16 type; s8 misc[30]; } OSScMsg;

#define OS_SC_RETRACE_MSG 1
#define OS_SC_PRE_NMI_MSG 4

VIEW(OSMesgQueue, model_thread_queue);
#define model_thread_queue model_thread_queue_view
VIEW(OSMesg[8], model_thread_msgs);
#define model_thread_msgs model_thread_msgs_view
VIEW(u16, modelrun);
#define modelrun modelrun_view
VIEW(u8[], gScheduler);
#define gScheduler gScheduler_view

extern void __setfpcsr(u32 value);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);

void ModelThread(void *arg)
{
    OSScClient client;
    OSMesg msg;

    msg = 0;
    __setfpcsr(0x01000E00);
    osCreateMesgQueue(&model_thread_queue, model_thread_msgs, 8);
    init_controllers();
    modelrun = 0;
    init_model_task(0);
    osScAddClient(gScheduler, &client, &model_thread_queue);
    while (1) {
        osRecvMesg(&model_thread_queue, &msg, 1);
        do {
            if (((OSScMsg *)msg)->type == OS_SC_RETRACE_MSG || ((OSScMsg *)msg)->type == OS_SC_PRE_NMI_MSG) {}
        } while (osRecvMesg(&model_thread_queue, &msg, 0) != -1);
        if (ControllerUpdate() && modelrun) {
            model_iteration();
        }
    }
}
