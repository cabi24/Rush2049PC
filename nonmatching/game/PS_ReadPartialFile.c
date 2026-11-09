#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {
    OSThread *mtqueue, *fullqueue;
    s32 validCount, first, msgCount;
    OSMesg *msg;
} OSMesgQueue;
typedef struct OSPfs {
    s32 status;
    OSMesgQueue *queue;
    s32 channel;
    u8 id[32], label[32];
    s32 version, dir_size, inode_table, minode_table, dir_table, inode_start_page;
    u8 banks, activebank;
} OSPfs;
typedef struct OSPfsState {
    u32 file_size, game_code;
    u16 company_code;
    u8 ext_name[4], game_name[16];
} OSPfsState;
/* one Controller Pak file slot (40 bytes; offsets from retail) */
typedef struct PakFile {
    s8 flush, flag1, flag2, opaque3;
    u16 mapBytes, opaque6;
    OSPfsState state;
} PakFile;
/* gPaks[pak] (GC anon_changed_inserted_25c; N64 772 bytes) */
typedef struct PakRecord {
    u8 changed;
    s8 inserted, opaque2, opaque3, opaque4, damaged, opaque6, flushing, flushed, opaque9, opaque10, flush;
    OSPfs pfs;
    s32 error, FreeSpace;
    u8 motorOn, request, level, opaque127;   /* rumble thread state (rumble_thread) */
    s16 phase;
    u8 opaque130[2];
    PakFile files[16];
} PakRecord;
typedef struct tPS_File tPS_File;
typedef struct PakHandle { tPS_File *file; } PakHandle;
/* GC tPS_File (N64 76 bytes): ll, RemoveFile, MoveFile, pak, file number, name/ext, size, blocks, data */
struct tPS_File {
    PakHandle *next;
    PakHandle *prev;                  /* PC: a pointer (InsertLLE's link); the record is 96 bytes (pc_prelude.h) */
    void (*RemoveFile)(PakHandle *);
    void (*MoveFile)(PakHandle *, PakHandle *);
    u8 pak, fileNo;
    u8 name[35], ext[11];
    u32 size, blocks;
    u8 **data;
};
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    u32 count;
    PakHandle *head, *tail;
} tDLList;

VIEW(volatile PakRecord[], gPaks);
#define gPaks gPaks_view
VIEW(tDLList[], gPS_Files);
#define gPS_Files gPS_Files_view
VIEW(tDLList, pak_free_handles);
#define pak_free_handles pak_free_handles_view

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

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

/* retail stub 0x800A2D44: take a tPS_File handle off the free list (compiled-out head test: see the header) */
PakHandle *pfs_get_node(void)
{
    PakHandle *fileh;
    if (pak_free_handles.head) {}
    fileh = pak_free_handles.head;
    RemoveLLE(&pak_free_handles, fileh);
    return fileh;
}

/* 0x800A2D4C */

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

void pdf_heap_lock(void)
{
    osRecvMesg(&heap_lock_queue, 0, 1);
}
void pdf_heap_unlock(void)
{
    osSendMesg(&heap_lock_queue, 0, 0);
}
void pdf_dealloc(u32 address)
{
    pdf_heap_lock();
    DeallocateMemoryInternal(address, 0);
    pdf_heap_unlock();
}
void pdf_free_blocks(s32 pak)
{
    osPfsFreeBlocks((OSPfs *)&gPaks[pak].pfs, (s32 *)&gPaks[pak].FreeSpace);
}
void pdf_release(PakHandle *fileh)
{
    RemoveLLE(&gPS_Files[fileh->file->pak], fileh);
    InsertLLE(&pak_free_handles, fileh, pak_free_handles.head);
}

extern void osInvalDCache(void *, s32);

void prpf_nop(void)
{
}

/* 0x800A2990: PS_ReadPartialFile(fileh, offset, numBytes) */
/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)

void PS_ReadPartialFile(PakHandle *fileh, s32 offset, u32 numBytes)
{
    tPS_File *psfile;
    s32 pak;
    PakHandle *cur;
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
    osInvalDCache(*psfile->data, psfile->size);
    prpf_nop();
    paksys_lock();
    size = numBytes;
    do {} while (0);
read_retry:
    error = osPfsReadWriteFile((OSPfs *)&gPaks[pak].pfs, fileNo, 0, offset,
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
                    pdf_dealloc(N64A(psfile->data));
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

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

/* 0x800A3640: _RemovePak (private IPA: unsaved s-registers) */

/* 0x800A3724: _ClearPak (private IPA: pak in a3) */

/* inlined: does the file's write bitmap have dirty bytes */
s32 _FileChanged(tPS_File *psfile)
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
    gPaks[pak].opaque6 = 0;
    gPaks[pak].phase = 0;
    gPaks[pak].level = 0;
    gPaks[pak].request = 0;
    gPaks[pak].motorOn = 0;
}

/* 0x800A377C: PS_PakServer */

/* 0x800A4508: PS_InitPakSys */
