/* Writing pak files back (MAT3 paksysdc.c, the N64's libultra form of it): the dirty maps, PS_WriteFile,
 * PS_FlushFile and PS_FlushPak, and the serial bus lock they take. */
#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread_s OSThread;

/* libultra's OSPfs (PcPfs's layout). */
typedef struct OSPfs {
    s32 status;
    OSMesgQueue *queue;
    s32 channel;
    u8 id[32], label[32];
    s32 version, dir_size, inode_table, minode_table, dir_table;
    s32 inode_start_page;
    u8 banks, activebank;
} OSPfs;
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern void osWritebackDCache(void *, s32);

/* Each pak's open files (MAT3 tDLList): head the first. */
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    u32 count;
    tPakHandle *head, *tail;
} tDLList;
VIEW(tDLList[], gPS_Files);
#define gPS_Files gPS_Files_view

VIEW(void (*)(s32,  s32,  s32,  s32,  s8 *,  s8 *,  s32), gErrorFunc);
#define gErrorFunc gErrorFunc_view

#define DIRTY_RUN 32   /* the dirty map's grain: one bit for each 32 bytes of a file */

/* Makes the serial bus lock (a one-message queue) the first time it is needed. */
void pfs_queue_init(void) {
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue, &si_lock_msg, 1);
        osSendMesg(&si_lock_queue, 0, 0);
    }
}

/* Takes the serial bus lock (si_lock's twin); si_unlock gives it back. */
void pfs_lock(void) {
    OSMesg message;
    pfs_queue_init();
    osRecvMesg(&si_lock_queue, &message, 1);
}

/* Clears pak index's flush flag when none of its open files is dirty any more. */
void ClearFlush(s32 index) {
    tPakHandle *p, *next;
    PakFileRecord *data;

    if (gPaks[index].flush == 0) { next = p = 0; goto out; }
    next = p = gPS_Files[index].head;
    while (p != 0) {
        data = p->file;
        next = data->next;
        if (data->data != 0) {
            if (gPaks[index].files[data->fileNo].dirty != 0) { next = 0; break; }
        }
        p = next;
    }
    if (p == 0) gPaks[index].flush = 0;
out:
    if (next) {}
}

/* Finds the first run of dirty 32-byte pieces in the file's data (its dirty map follows the data): its address, its
 * size in bytes and its offset in the file. Returns 1 if there is one, 0 when the file is clean or not read. */
s32 pak_find_dirty_run(tPakHandle *handle, u8 **address, s32 *bytes, s32 *offset) {
    u8 complete = 0;
    PakFileRecord *file = handle->file;
    u8 *map;
    u32 i, j;
    if (!file->data) return 0;
    *bytes = 0;
    map = *file->data + file->size;
    for (i = 0; i < gPaks[file->pak].files[file->fileNo].mapBytes; i++) {
        if (map[i] != 0) {
        for (j = 0; j < 8; j++) {
            if (map[i] & (1 << j)) {
                if (*bytes == 0) {
                    *offset = (i * 8 + j) * DIRTY_RUN;
                    *address = *file->data + *offset;
                    *bytes = 1;
                } else (*bytes)++;
            } else if (*bytes) {
                complete = 1;
                break;
            }
        }
        if (complete) break;
        }
    }
    *bytes *= DIRTY_RUN;
    return (*bytes == 0) ^ 1;
}

/* Marks (mark set) or clears the dirty bits of the 32-byte pieces that bytes bytes at address cover in the file's
 * data. (Nothing checks that address lies in the file.) */
void pak_mark_dirty(tPakHandle *handle, u8 *address, u32 bytes, s32 mark) {
    PakFileRecord *file = handle->file;
    u8 *map;
    u32 first, last, i;
    u8 *end;
    if (address < *file->data || address >= *file->data + file->size) {
    }
    end = address + bytes;
    bytes = (u32)(address - *file->data);
    first = bytes;
    bytes >>= 5;
    last = (u32)(end - *file->data - 1) >> 5;
    map = *file->data + file->size;
    for (i = bytes; i <= last; i++) {
        if (mark) map[i >> 3] |= 1 << (i & 7);
        else map[i >> 3] &= ~(1 << (i & 7));
    }
    if (first) {}
}

/* Writes the dirty runs of the file to its pak, the caller holding the serial bus lock, and marks it clean. A write
 * that fails: with mode 0 the error function asks the player (a Rumble Pak, a pak that isn't the original or no pak
 * become kPS_ErrNewPak or kPS_ErrNoPak) and the write is retried if the player asks, else the file is marked clean
 * anyway; with mode set it stays dirty. Returns 1 when written (or nothing to write), 0 on a failure. */
s32 PS_FlushFileInternal(tPakHandle *handle, s32 mode) {
    s32 result;
    s32 index, slot_index;
    s8 retry, callback_value;
    u8 *address;
    s32 bytes, offset;
    u8 **allocation;
    if (!handle) return 1;
    index = handle->file->pak;
    slot_index = handle->file->fileNo;
    allocation = handle->file->data;
    if (!gPaks[index].files[slot_index].dirty) return 1;
    osWritebackDCache(*allocation, handle->file->size);
    if (pak_find_dirty_run(handle, &address, &bytes, &offset)) {
        do {
            osWritebackDCache(address, bytes);
write_retry:
            result = osPfsReadWriteFile(&gPaks[index].pfs, slot_index, 1, offset, bytes, address);
            if (result) {
                gPaks[index].error = pak_error_from_pfs(result);
                if (!mode) {
                    if (gPaks[index].rumble) gPaks[index].error = kPS_ErrNewPak;
                    else if (gPaks[index].inserted && !gPaks[index].original) gPaks[index].error = kPS_ErrNewPak;
                    else if (!gPaks[index].inserted) gPaks[index].error = kPS_ErrNoPak;
                    si_unlock();
                    gErrPak = index;
                    gErrorFunc(index, gPaks[index].error, 0, 0, &retry, &callback_value, 0);
                    gErrPak = -1;
                    pfs_lock();
                    if (retry) goto write_retry;
                }
                if (!mode) {
                    gPaks[index].files[slot_index].dirty = 0;
                    ClearFlush(index);
                }
                return 0;
            }
            pak_mark_dirty(handle, address, bytes, 0);
        } while (pak_find_dirty_run(handle, &address, &bytes, &offset));
    }
    gPaks[index].files[slot_index].dirty = 0;
    ClearFlush(index);
    return 1;
}

/* Writes every dirty open file of pak index (PS_FlushFileInternal), stopping at the first failure. flushed says
 * whether anything was written. Returns 1 when all were written. */
s32 PS_FlushPak(s32 index, s32 mode) {
    tPakHandle *node;
    tPakHandle *next;
    gPaks[index].flushed = 0;
    if (!gPaks[index].flush) return 1;
    pfs_lock();
    gPaks[index].flushing = 1;
    node = gPS_Files[index].head;
    while (node) {
        next = node->file->next;
        if (node->file->data && gPaks[index].files[node->file->fileNo].dirty) {
            if (!PS_FlushFileInternal(node, mode)) break;
            gPaks[index].flushed = 1;
        }
        node = next;
    }
    gPaks[index].flushing = 0;
    si_unlock();
    return node == 0;
}

/* Writes the file's dirty runs to its pak (PS_FlushFileInternal under the lock). Returns 1 when written. */
s32 PS_FlushFile(tPakHandle *handle, s32 mode) {
    pfs_lock();
    gPaks[handle->file->pak].flushing = 1;
    gPaks[handle->file->pak].flushed = 0;
    gPaks[handle->file->pak].flushed = PS_FlushFileInternal(handle, mode);
    gPaks[handle->file->pak].flushing = 0;
    si_unlock();
    return gPaks[handle->file->pak].flushed;
}

/* Notes that bytes bytes at address in the file's data have changed: they become dirty, and so do the file and its
 * pak, for the next flush to write. Not in include/game: its callers pass the address as a pointer or an integer. */
void PS_WriteFile(tPakHandle *handle, u8 *address, u32 bytes) {
    PakFileRecord *file;
    if (!handle) return;
    file = handle->file;
    if (gPaks[file->pak].files[file->fileNo].flag2) {}
    pak_mark_dirty(handle, address, bytes, 1);
    gPaks[file->pak].files[file->fileNo].dirty = 1;
    gPaks[file->pak].flush = 1;
}
