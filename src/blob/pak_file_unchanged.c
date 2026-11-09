/* pak_file_unchanged: whether a pak file is still the one read (MAT3 paksysdc.c's _FileChanged, inverted). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg ( OSMesgQueue * , OSMesg , s32 ), osRecvMesg ( OSMesgQueue * , OSMesg * , s32 );

/* Whether the pak is in. */
static s32 pak_enabled(tPak *pak)
{
    return pak->inserted;
}

/* Makes the serial bus lock the first time it is needed (a copy of pfs_queue_init). */
static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue,&si_lock_msg,1);
        osSendMesg(&si_lock_queue,0,0);
    }
}

/* Takes the serial bus lock (a copy of si_lock). */
static void pak_lock(void)
{
    OSMesg message;

    pak_queue_init();
    osRecvMesg(&si_lock_queue,&message,1);
}

/* 1 when the file's directory entry on the pak is still the one the pak system read (so no other console wrote it
 * meanwhile), 0 when it differs, can't be read or the pak is out. */
int pak_file_unchanged(tPakHandle *handle)
{
    tPak *pak;
    u32 port,file;
    int error;
    PakFileState state;
    port = handle->file->pak;
    file = handle->file->fileNo;
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
