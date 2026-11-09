/* The pak system's file and pak management (MAT3 paksysdc.c, the N64's libultra form of it): creating and deleting
 * files, forgetting a pak's files, the pak server that notices paks being put in and taken out, and the start-up. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {
    OSThread *mtqueue, *fullqueue;
    s32 validCount, first, msgCount;
    OSMesg *msg;
} OSMesgQueue;
/* libultra's OSPfs, as the local copy of a pak's handle (PcPfs's layout). */
typedef struct OSPfs {
    s32 status;
    OSMesgQueue *queue;
    s32 channel;
    u8 id[32], label[32];
    s32 version, dir_size, inode_table, minode_table, dir_table, inode_start_page;
    u8 banks, activebank;
} OSPfs;
/* A pak's open files (MAT3 tDLList), linked through their records' next. */
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    u32 count;
    tPakHandle *head, *tail;
} tDLList;
typedef s32 (*PakPredicate)(s32);
typedef void (*PakErrorFunc)(s32, s32, u32, s32, s8 *, s8 *, PakPredicate);

VIEW(volatile tPak[], gPaks);
#define gPaks gPaks_view
VIEW(tDLList[], gPS_Files);
#define gPS_Files gPS_Files_view
VIEW(tDLList, pak_free_handles);
#define pak_free_handles pak_free_handles_view

extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);

extern void *memset(void *, s32, u32);

/* Puts the open file fileh at the head of its pak's list of files. */
void pak_insert_file(tPakHandle *fileh)
{
    InsertLLE(&gPS_Files[fileh->file->pak], fileh, gPS_Files[fileh->file->pak].head);
}

/* Takes a handle off the free list (pak_free_handles); it lives in the system group. */
tPakHandle *pfs_get_node(void);

/* Creates the file name.ext of numBytes bytes (rounded up to whole pages) on pak's pak, for company and gameCode, and
 * opens it: its data, all zeros, and its dirty map, all clean, are allocated in the heap. A failure goes to the error
 * function (gErrorFunc), which asks the player: retry, or give up (NULL). When the file exists and the player retries,
 * the open handle of the existing file is deleted first. Returns the new file's handle, or NULL. */
tPakHandle *PS_CreateFile(s32 pak, u8 *name, u8 *ext, u32 numBytes, s32 company, s32 gameCode)
{
    s32 error;
    s32 file;
    u8 game_name[16];
    u8 ext_name[4];
    s8 retry;
    s8 status;
    tPakHandle *fileh;
    PakFileRecord *psfile;
    PakFileState *state;

    paksys_lock();
    pak_encode_name(game_name, name, 16);
    pak_encode_name(ext_name, ext, 4);
    for (;;) {
        error = osPfsAllocateFile(&gPaks[pak].pfs, (u16)company, (u32)gameCode,
                                  game_name, ext_name, (numBytes + 255) & ~255U, &file);
        if (error) {
            gPaks[pak].error = pak_error_from_pfs(error);
            si_unlock();
            gErrPak = pak;
            gErrorFunc(pak, gPaks[pak].error, (numBytes + 255) >> 8, 0, &retry, &status,
                       gPaks[pak].error == kPS_ErrNewPak ? 0 : pak_removed);
            gErrPak = -1;
            if (gPaks[pak].error != kPS_ErrNewPak && !gPaks[pak].inserted) return 0;
            paksys_lock();
            if (gPaks[pak].error == kPS_ErrExist && retry) {
                /* the file exists: find its slot and delete our handle for it */
                for (file = 0; file < PAK_MAX_FILES; file++) {
                    state = &gPaks[pak].files[file].state;
                    if (game_strcmp(state->game_name, game_name) == 0 &&
                        game_strcmp(state->ext_name, ext_name) == 0 &&
                        state->game_code == (u32)gameCode &&
                        company == state->company_code) break;
                }
                if (file == PAK_MAX_FILES) continue;
                si_unlock();
                for (fileh = gPS_Files[pak].head; fileh != 0; fileh = fileh->file->next) {
                    if (fileh->file->fileNo == file) {
                        PS_DeleteFile(fileh);
                        break;
                    }
                }
                paksys_lock();
            }
            if (gPaks[pak].error == kPS_ErrFull && retry) retry = 0;
            if (!retry) {
                si_unlock();
                return 0;
            }
        } else break;
    }
    osPfsFreeBlocks(&gPaks[pak].pfs, &gPaks[pak].FreeSpace);
    for (;;) {
        error = osPfsFileState(&gPaks[pak].pfs, file, &gPaks[pak].files[file].state);
        if (!error && gPaks[pak].files[file].state.file_size != 0) break;
        gPaks[pak].error = pak_error_from_pfs(error);
        si_unlock();
        gErrPak = pak;
        gErrorFunc(pak, gPaks[pak].error, (numBytes + 255) >> 8, 0,
                   &retry, &status, pak_removed);
        gErrPak = -1;
        if (!gPaks[pak].inserted) {
            gPaks[pak].files[file].state.file_size = 0;
            return 0;
        }
        paksys_lock();
        if (!retry) {
            si_unlock();
            gPaks[pak].files[file].state.file_size = 0;
            return 0;
        }
    }
    fileh = pfs_get_node();
    psfile = fileh->file;
    psfile->RemoveFile = 0;
    psfile->MoveFile = 0;
    psfile->pak = pak;
    psfile->fileNo = file;
    psfile->size = gPaks[pak].files[file].state.file_size;
    psfile->blocks = (psfile->size + 255) >> 8;
    gPaks[pak].files[file].mapBytes = (((psfile->size + 31) >> 5) + 7) >> 3;   /* a bit for each 32 bytes */
    psfile->data = AllocateHandleHeap(0, gPaks[pak].files[file].mapBytes + psfile->size);
    memset(*psfile->data, 0, psfile->size);
    memset(*psfile->data + psfile->size, 255, gPaks[pak].files[file].mapBytes);
    gPaks[pak].files[file].locked = 0;
    gPaks[pak].files[file].dirty = 0;
    gPaks[pak].files[file].flag2 = 0;
    pak_decode_name(psfile->name, gPaks[pak].files[file].state.game_name, 16);
    pak_decode_name(psfile->ext, gPaks[pak].files[file].state.ext_name, 4);
    pak_insert_file(fileh);
    si_unlock();
    return fileh;
}

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

/* Deletes the open file fileh from its pak and closes it: its RemoveFile is told, its data freed, its handle given
 * back. A failure goes to the error function; the deletion is retried if the player asks and the file is still open. */
void PS_DeleteFile(tPakHandle *fileh)
{
    s32 pak;
    s32 fileNo;
    PakFileState *state;
    tPakHandle *cur;
    s8 retry;
    s8 status;
    PakFileRecord *psfile;
    s32 error;

    paksys_lock();
    psfile = fileh->file;
    pak = psfile->pak;
    fileNo = psfile->fileNo;
    state = (PakFileState *)&gPaks[pak].files[fileNo].state;
delete_retry:
    error = osPfsDeleteFile((OSPfs *)&gPaks[pak].pfs, state->company_code, state->game_code,
                            state->game_name, state->ext_name);
    if (error) {
        gPaks[pak].error = pak_error_from_pfs(error);
        si_unlock();
        gErrPak = pak;
        gErrorFunc(pak, gPaks[pak].error, 0, 0, &retry, &status, pak_removed);
        gErrPak = -1;
        for (cur = gPS_Files[pak].head; cur != 0; cur = cur->file->next) {
            if (fileh == cur) break;
        }
        if (cur == 0) return;
        paksys_lock();
        if (retry) goto delete_retry;
        si_unlock();
        return;
    }
    pdf_free_blocks(pak);
    if (psfile->RemoveFile) psfile->RemoveFile(fileh);
    state->file_size = 0;
    if (psfile->data) {
        pdf_dealloc(PC_N64A(psfile->data));
        psfile->data = 0;
    }
    pdf_release(fileh);
    si_unlock();
}

/* ---- PS_PakServer and its private callees ---- */
typedef struct ControllerData { s8 present; u8 opaque1[15]; } ControllerData;
VIEW(ControllerData[], gControllerData);
#define gControllerData gControllerData_view

VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view
VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

extern void *memcpy(void *, const void *, u32);

/* Forgets every open file of pak's pak (MAT3 _RemovePak): each file's RemoveFile is told, its data freed under the
 * heap lock, its handle given back to the free list. */
void RemovePak(s32 pak)
{
    tPakHandle *fileh;
    tPakHandle *next;
    PakFileRecord *psfile;

    fileh = gPS_Files[pak].head;
    while (fileh != 0) {
        psfile = fileh->file;
        next = psfile->next;
        if (psfile->RemoveFile != 0) {
            psfile->RemoveFile(fileh);
        }
        if (psfile->data != 0) {
            u8 **data = psfile->data;
            osRecvMesg(&heap_lock_queue, 0, 1);
            DeallocateMemoryInternal(PC_N64A(data), 0);
            osSendMesg(&heap_lock_queue, 0, 0);
            psfile->data = 0;
        }
        RemoveLLE(&gPS_Files[fileh->file->pak], fileh);
        InsertLLE(&pak_free_handles, fileh, pak_free_handles.head);
        fileh = next;
    }
}

/* Clears pak's record to zeros but for its damaged flag (MAT3 _ClearPak). */
void ClearPak(u8 pak)
{
    s8 damaged;

    damaged = gPaks[pak].damaged;
    memset((void *)&gPaks[pak], 0, sizeof(tPak));
    gPaks[pak].damaged = damaged;
}

/* Looks at every controller's pak (only gErrPak's while an error screen is up) and keeps gPaks up to date: a pak
 * taken out has its files forgotten (unless they are locked or being written: they wait for it to come back); a pak
 * put in is read (osPfsInitPak): a Rumble Pak is noted (with rumbleOnly nothing else is done), an unreadable one goes
 * to the error screen (kPS_ErrRecognize: repair it, give up, or read it again), then the pak is compared with the
 * one before by its id: the same pak keeps its files (the changed ones marked to write), a pak another port had
 * takes that port's files, any other has its dirty files written first (PS_FlushPak) and its directory read afresh,
 * each file there opened (without its data). tPak.changed says which paks changed. forceUpdate is unused. */
void PS_PakServer(s32 forceUpdate, s32 rumbleOnly)
{
    s32 i;
    s32 j;
    s32 ret;
    s32 err;                /* unused */
    OSPfs pfs;
    s8 X;                   /* unused */
    s8 status = 0;
    s8 retry = 0;
    s8 changed[4];
    s8 dup;
    s8 same;
    tPakHandle *fileh;
    PakFileRecord *psfile;

    if (!gPakSysInited) return;
    for (i = 0; i < NUM_PAKS; i++) changed[i] = 0;
    si_lock();
    for (i = 0; i < NUM_PAKS; i++) {
        if (gErrPak >= 0 && i != gErrPak) continue;
        if (!gControllerData[i].present || !gPaks[i].plugged || gPaks[i].wasRemoved) {
            /* no pak: forget it */
            gPaks[i].wasRemoved = 0;
            if (gPaks[i].inserted) {
                gPaks[i].inserted = 0;
                gPaks[i].original = 0;
                gPaks[i].error = 0;
                if (gPaks[i].rumble) {
                    _StopRumble(i);
                    changed[i] = 1;
                    continue;
                }
                if (gPaks[i].locked) goto rj;
                if (!gPaks[i].flush) {
                    if (gPaks[i].flushing) goto rj;
                    RemovePak(i);
                    ClearPak(i);
                }
rj:
                gPaks[i].damaged = 0;
                changed[i] = 1;
            } else if (gPaks[i].inited && !gPaks[i].locked && !gPaks[i].flush && !gPaks[i].flushing) {
                RemovePak(i);
                ClearPak(i);
            }
            continue;
        }
        if (gPaks[i].inserted) continue;
        /* a pak was put in: read it */
        changed[i] = 1;
        gPaks[i].inserted = 1;
        dup = 0; same = 0; memcpy(&pfs, (void *)&gPaks[i].pfs, sizeof(OSPfs));
init:
        pfs.queue = 0;
        ret = osPfsInitPak(&si_event_queue, &pfs, i);
        if (!pfs.queue) ret = 1;
        gPaks[i].error = pak_error_from_pfs(ret);
        if (gPaks[i].error == kPS_ErrRecognize) {
            if (!osMotorInit(&si_event_queue, &pfs, i)) {
                if (!gPaks[i].rumble) gPaks[i].rumble = 1;
                else changed[i] = 0;
                continue;
            }
            if (rumbleOnly) {
                gPaks[i].inserted = 0;
                changed[i] = 0;
                continue;
            }
            si_unlock();
prompt:
            gErrPak = i;
            gErrorFunc(i, kPS_ErrRecognize, 0, 0, &retry, &status, pak_removed);
            gErrPak = -1;
            if (!gPaks[i].inserted) {
                si_lock();
                changed[i] = 0;
                continue;
            }
            if (status) {
                /* repair it */
                si_lock();
                ret = osPfsInitPak(&si_event_queue, &pfs, i);
                if (!ret) goto inited;
                ret = osPfsRepairId(&pfs);
                si_unlock();
                if (ret) {
                    gErrPak = i;
                    gErrorFunc(i, kPS_ErrRepairFailed, 0, 0, &retry, &status, 0);
                    gErrPak = -1;
                    goto prompt;
                }
                gErrPak = i;
                gErrorFunc(i, kPS_ErrRepaired, 0, 0, &retry, &status, 0);
                gErrPak = -1;
            } else if (retry) {
                /* give up: the pak is damaged */
                si_lock();
                gPaks[i].damaged = 1;
                gPaks[i].original = 0;
                changed[i] = 0;
                continue;
            } else {
                gErrPak = i;
                gErrorFunc(i, kPS_ErrReread, 0, 0, &retry, &status, 0);
                gErrPak = -1;
            }
            si_lock();
            goto init;
        }
inited:
        gPaks[i].rumble = 0;
        gPaks[i].phase = 0;
        gPaks[i].level = 0;
        gPaks[i].request = 0;
        gPaks[i].motorOn = 0;
        if (ret == 2) gPaks[i].error = 0;   /* PFS_ERR_NEW_PACK */
        else if (ret) {
            changed[i] = 0;
            gPaks[i].damaged = 1;
            gPaks[i].original = 0;
            continue;
        }
        if (!gPaks[i].inited) {
            gPaks[i].inited = 1;
        } else if (!game_memcmp((u8 *)gPaks[i].pfs.id, pfs.id, 32)) {
            /* the same pak as before: keep its files, marking those changed meanwhile to write */
            changed[i] = 0;
            memcpy((void *)&gPaks[i].pfs, &pfs, sizeof(OSPfs));
            osPfsFreeBlocks((OSPfs *)&gPaks[i].pfs, (s32 *)&gPaks[i].FreeSpace);
            gPaks[i].original = 1;
            if (!gPaks[i].flushing) {
                for (fileh = gPS_Files[i].head; fileh; fileh = fileh->file->next) {
                    if (_FileChanged(fileh->file)) {
                        gPaks[i].files[fileh->file->fileNo].dirty = 1;
                        gPaks[i].flush = 1;
                    }
                }
                same = 1;
                goto flush;
            }
            continue;
        }
        for (j = 0; j < NUM_PAKS; j++) {
            if (i == j) continue;
            if (gPaks[j].inited && !game_memcmp(pfs.id, (u8 *)gPaks[j].pfs.id, 32)) {
                /* the pak port j had: its files move here */
                if (gPaks[i].inited) {
                    dup = 1;
                    goto locked;
                }
move:
                memcpy((void *)&gPaks[i].pfs, &pfs, sizeof(OSPfs));
                osPfsFreeBlocks((OSPfs *)&gPaks[i].pfs, (s32 *)&gPaks[i].FreeSpace);
                gPaks[i].original = 1;
                memcpy(&gPS_Files[i], &gPS_Files[j], sizeof(tDLList));
                ClearLL(&gPS_Files[j]);
                for (fileh = gPS_Files[i].head; fileh; fileh = fileh->file->next)
                    fileh->file->pak = i;
                memcpy((void *)gPaks[i].files, (void *)gPaks[j].files, sizeof(gPaks[i].files));
                changed[i] = 0;
                gPaks[j].inited = 0;
                gPaks[j].original = 0;
                if (gPaks[j].inserted && !gPaks[j].damaged) gPaks[j].wasRemoved = 1;
                changed[j] = 1;
                break;
            }
        }
        if (j < NUM_PAKS) continue;
        if (gPaks[i].flushing) {
            changed[i] = 0;
            continue;
        }
locked:
        if (gPaks[i].locked) {
            changed[i] = 0;
            continue;
        }
flush:
        /* write what the files before had unwritten, then read the new pak's directory */
        si_unlock();
        if (!PS_FlushPak(i, 0)) {
            changed[i] = 0;
            si_lock();
            continue;
        }
        si_lock();
        if (gPaks[i].flushed && !same) {
            dup = 0;
            same = 0;
            goto init;
        }
        if (same) continue;
        RemovePak(i);
        if (dup) goto move;
        gPaks[i].original = 1;
        gPaks[i].locked = 0;
        gPaks[i].flush = 0;
        memcpy((void *)&gPaks[i].pfs, &pfs, sizeof(OSPfs));
        osPfsFreeBlocks((OSPfs *)&gPaks[i].pfs, (s32 *)&gPaks[i].FreeSpace);
        for (j = 0; j < PAK_MAX_FILES; j++) {
            ret = osPfsFileState((OSPfs *)&gPaks[i].pfs, j, (PakFileState *)&gPaks[i].files[j].state);
            if (ret) gPaks[i].files[j].state.file_size = 0;
            gPaks[i].files[j].locked = 0;
            gPaks[i].files[j].dirty = 0;
            gPaks[i].files[j].flag2 = 0;
            gPaks[i].files[j].mapBytes = (((gPaks[i].files[j].state.file_size + 31) >> 5) + 7) >> 3;
            gPaks[i].error = pak_error_from_pfs(ret);
            if (gPaks[i].files[j].state.file_size) {
                fileh = pfs_get_node();
                psfile = fileh->file;
                psfile->RemoveFile = 0;
                psfile->MoveFile = 0;
                psfile->pak = i;
                psfile->fileNo = j;
                psfile->size = gPaks[i].files[j].state.file_size;
                psfile->blocks = (psfile->size + 255) >> 8;
                pak_decode_name(psfile->name, (u8 *)gPaks[i].files[j].state.game_name, 16);
                pak_decode_name(psfile->ext, (u8 *)gPaks[i].files[j].state.ext_name, 4);
                if (psfile->ext[0]) psfile->ext[1] = 0;
                psfile->data = 0;
                pak_insert_file(fileh);
            }
        }
    }
    si_unlock();
    for (i = 0; i < NUM_PAKS; i++) gPaks[i].changed = changed[i];
    if (forceUpdate) {}
}

VIEW(tPakHandle[], pak_file_handles);
#define pak_file_handles pak_file_handles_view
VIEW(PakFileRecord[], pak_file_records);
#define pak_file_records pak_file_records_view

extern void osCreateThread(void *, s32, void (*)(void *), void *, void *, s32);
extern void osStartThread(void *);

/* Starts the pak system: errorFunc is what asks the player when a pak fails (PS_ErrorFunc); the 64 file handles go
 * on the free list; every pak record is cleared (keeping no damage: each is cleared to undamaged); the rumble thread
 * starts; then the paks in are noted (pak_poll_inserted). */
void PS_InitPakSys(PakErrorFunc errorFunc)
{
    s32 i;

    gErrorFunc = errorFunc;
    InitLL(&pak_free_handles, 1, 1);
    for (i = 0; i < 64; i++) {
        pak_file_handles[i].file = &pak_file_records[i];
        InsertLLE(&pak_free_handles, &pak_file_handles[i], pak_free_handles.head);
    }
    for (i = 0; i < NUM_PAKS; i++) {
        InitLL(&gPS_Files[i], 1, 1);
        gPaks[i].damaged = 0;
        ClearPak(i);
    }
    osCreateThread(rumble_thread, 2, rumble_thread_entry, 0, rumble_thread_stack_top, 6);
    osStartThread(rumble_thread);
    gPakSysInited = 1;
    pak_poll_inserted();
}
