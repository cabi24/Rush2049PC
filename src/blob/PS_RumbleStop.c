#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;
typedef struct OSPfs {s32 status;OSMesgQueue *queue;s32 channel;u8 id[32],label[32];s32 version,dir_size,inode_table,minode_table,dir_table,inode_start_page;u8 banks,activebank;} OSPfs;
typedef struct OSPfsState {u32 file_size,game_code;u16 company_code;char ext_name[4],game_name[16];} OSPfsState;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);
typedef struct PakFile40 { u32 metadata;u16 count,flags;OSPfsState state; } PakFile40;
typedef struct Pak772 {
    u8 index;s8 enabled;u8 opaque2[4];s8 present;u8 opaque[5];
    OSPfs pfs;
    u8 opaque116[8];
    u8 motorStopped;
    u8 motorOn;
    u8 opaque126[6];
    PakFile40 files[16];
} Pak772;
VIEW(Pak772[], gPaks);
#define gPaks gPaks_view

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view

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

static s32 motor_init(Pak772 *p, s32 i)
{
    if (!p->present) return -1;
    p->motorOn = 0;
    osMotorInit(&si_event_queue, &p->pfs, i);
    if (__osMotorAccess(&p->pfs, 0) == 0) {
        p->motorStopped = 0;
    }
}

void PS_RumbleStop(void)
{
    s32 i;
    Pak772 *p;

    gRumble = 0;
    pak_lock();
    for (i = 0; i < 4; i++) {
        p = &gPaks[i];
        motor_init(p, i);
    }
    osSendMesg(&si_lock_queue,0,0);
}
