#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;
typedef struct OSPfs {s32 status;OSMesgQueue *queue;s32 channel;u8 id[32],label[32];s32 version,dir_size,inode_table,minode_table,dir_table,inode_start_page;u8 banks,activebank;} OSPfs;
typedef struct OSPfsState {u32 file_size,game_code;u16 company_code;char ext_name[4],game_name[16];} OSPfsState;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg ( OSMesgQueue * , OSMesg , s32 ), osRecvMesg ( OSMesgQueue * , OSMesg * , s32 );
typedef struct PakFile40 { u32 metadata;u16 count,flags;OSPfsState state; } PakFile40;
typedef struct Pak772 {
    u8 index;s8 enabled;u8 opaque[10];
    OSPfs pfs;
    u8 opaque116[16];
    PakFile40 files[16];
} Pak772;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Object {void *next,*prev;void (*RemoveFile)(void *),(*MoveFile)(void *,void *);u8 port,file;} Object;
typedef struct Handle {Object *object;} Handle;
VIEW(Pak772[], gPaks);
#define gPaks gPaks_view

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

static s32 pak_enabled(Pak772 *pak)
{
    return pak->enabled;
}

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

int pak_file_unchanged(Handle *handle)
{
    Pak772 *pak;
    u32 port,file;
    int error;
    OSPfsState state;
    port = handle->object->port;
    file = handle->object->file;
    pak = &gPaks[port];
    if (!pak_enabled(pak)) return 0;
    pak_lock();
    error = osPfsFileState(&pak->pfs,file,&state);
    if (error) {}
    si_unlock();
    if (error) return 0;
    if (game_memcmp(&state,&gPaks[port].files[file].state,32)) return 0;
    return 1;
}
