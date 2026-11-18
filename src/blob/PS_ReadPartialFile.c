#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {
    OSThread *mtqueue, *fullqueue;
    s32 validCount, first, msgCount;
    OSMesg *msg;
} OSMesgQueue;

extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);

extern void *memset(void *, s32, u32);

/* the inlined pak lock helpers (bodies as group pak_write_cluster pfs_*; own names: the unit renames duplicated internal names) */
void paksys_queue_init(void) {
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue, &si_lock_msg, 1);
        osSendMesg(&si_lock_queue, 0, 0);
    }
}
void paksys_lock(void) {
    OSMesg message;
    paksys_queue_init();
    osRecvMesg(&si_lock_queue, &message, 1);
}

/* Takes a file handle off the free list (pak_free_handles). The empty test of the list's head is what is left of the
 * original's check, which compiled to nothing. */
tPakHandle *pfs_get_node(void)
{
    tPakHandle *fileh;
    if (pak_free_handles.head) {}
    fileh = pak_free_handles.head;
    RemoveLLE(&pak_free_handles, fileh);
    return fileh;
}

void pdf_heap_lock(void)
{
    osRecvMesg(&heap_lock_queue, 0, 1);
}
void pdf_heap_unlock(void)
{
    osSendMesg(&heap_lock_queue, 0, 0);
}
void pdf_dealloc(void *ptr)
{
    pdf_heap_lock();
    DeallocateMemoryInternal(ptr, 0);
    pdf_heap_unlock();
}
void pdf_free_blocks(s32 pak)
{
    osPfsFreeBlocks(&gPaks[pak].pfs, (s32 *)&gPaks[pak].FreeSpace);
}
void pdf_release(tPakHandle *fileh)
{
    RemoveLLE(&gPS_Files[fileh->file->pak], fileh);
    InsertLLE(&pak_free_handles, fileh, pak_free_handles.head);
}


void prpf_nop(void)
{
}

void PS_ReadPartialFile(tPakHandle *fileh, s32 offset, u32 numBytes)
{
    PakFileRecord *psfile;
    s32 pak;
    tPakHandle *cur;
    s32 error;
    s32 fileNo;
    s8 retry;
    s8 status;
    u32 size;

    psfile = fileh->file;
    pak = psfile->pak;
    fileNo = psfile->fileNo & 0xFF;
    if (psfile->data) return;
    if (offset > psfile->size) {}
    if (offset) {}
    psfile->data = AllocateHandleHeap(0, gPaks[pak].files[fileNo].mapBytes + psfile->size);
    memset(*psfile->data + psfile->size, 0, gPaks[pak].files[fileNo].mapBytes);
    prpf_nop();
    paksys_lock();
    size = numBytes;
    do {} while (0);
read_retry:
    error = osPfsReadWriteFile(&gPaks[pak].pfs, fileNo, 0, offset,
                               ((size + 31) >> 5) << 5, *psfile->data);
    if (error) {
        gPaks[pak].error = pak_error_from_pfs(error);
        if (gPaks[pak].error == 9) {
            si_unlock();
            PS_DeleteFile(fileh);
            return;
        }
        si_unlock();
        gErrPak = pak;
        gErrorFunc(pak, gPaks[pak].error, 0, 0, &retry, &status, pak_removed);
        gErrPak = -1;
        paksys_lock();
        if (retry) goto read_retry;
        for (cur = gPS_Files[pak].head; cur != 0; cur = cur->file->next) {
            if (fileh == cur) {
                if (psfile->data) {
                    pdf_dealloc(psfile->data);
                    psfile->data = 0;
                }
                break;
            }
        }
        si_unlock();
        return;
    }
    si_unlock();
    gPaks[pak].files[fileNo].flag2 = numBytes != psfile->size;
}

/* MAT3's _RemovePak and _ClearPak come next in paksysdc.c: they are RemovePak and ClearPak in
 * groups/paksysdc/group.c. */

/* inlined: does the file's write bitmap have dirty bytes */
s32 _FileChanged(PakFileRecord *psfile)
{
    u8 *map;
    u32 i;

    if (!psfile->data) return 0;
    map = *psfile->data + psfile->size;
    for (i = 0; i < gPaks[psfile->pak].files[psfile->fileNo].mapBytes; i++) {
        if (map[i]) return 1;
    }
    return 0;
}

/* inlined: motor off and rumble state cleared */
void _StopRumble(s32 pak)
{
    gPaks[pak].rumble = 0;
    gPaks[pak].phase = 0;
    gPaks[pak].level = 0;
    gPaks[pak].request = 0;
    gPaks[pak].motorOn = 0;
}

/* PS_PakServer and PS_InitPakSys, which come next in paksysdc.c, are in groups/paksysdc/group.c. */
