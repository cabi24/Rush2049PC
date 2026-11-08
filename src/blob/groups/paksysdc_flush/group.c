/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Controller-pak write cluster (-> ->): 7/7 EQUAL in the whole-program unit, the scorer MATCH.
 * N64-only (no arcade ancestor; GC paksysdc.c is a different platform layer). Members: ClearFlush (clear the
 * channel's modified flag when no open file is still dirty), pak_find_dirty_run (find the next dirty 32-byte run),
 * pak_mark_dirty (mark/clear the dirty bitmap for a byte range), PS_FlushFileInternal (write dirty runs with
 * osPfsReadWriteFile, retry via the error callback), track_collision_setup / MaxPathZeroControls (locked flush
 * wrappers), slot_state_lookup (mark a range dirty).
 * Unit command (repo root):
 * python3 -m the whole-program unit --tag score ClearFlush pak_find_dirty_run pak_mark_dirty \
 *     PS_FlushFileInternal track_collision_setup MaxPathZeroControls slot_state_lookup --with THIS.c \
 *     --internal ClearFlush --internal pak_find_dirty_run --internal pak_mark_dirty --internal PS_FlushFileInternal \
 *     --internal pfs_queue_init --internal pfs_lock --internal pfs_unlock --neighbours
 * Shaping devices (disclosed):
 * - `extern volatile C96Channel gPaks` (the pak records are volatile).
 *  - pak_mark_dirty: the empty range check `if (address < base || address >= base + size) {}` (from the research base;
 *    its loads survive and are retail's).
 *  - slot_state_lookup: `if (...slots[slot].flag2) {}` (compiled-out test of a volatile byte; emits only retail's lb).
 *  - pfs_queue_init/pfs_lock/pfs_unlock are non-static internal helpers (static ones leave deleted-procedure stubs).
 *  - ClearFlush: dead stores `next = p = 0` (early-return block, reached by `goto out`) and `next = 0` (found
 *    block), kept alive to ugen by the compiled-out holder `out: if (next) {}`; as1 deletes them as dead, which
 * leaves retail's tail-duplicated `jr ra; nop` early return and the `b exit; nop` found-path jump-around.
 *  - pak_mark_dirty: `first = bytes;` (the byte offset) is never used except by the compiled-out holder
 *    `if (first) {}` after the loop: a codeless web that occupies a3 (retail's skipped register); `bytes >>= 5`
 * pops the ugen temp that retail pops (dest == operand). Frame 0, so no frame cost.
 * Base: / game_C96 (types), bodies.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef struct OSThread_s OSThread;
typedef void *OSMesg;
typedef struct OSMesgQueue_s {
    OSThread *mtqueue, *fullqueue;
    s32 validCount, first, msgCount;
    OSMesg *msg;
} OSMesgQueue;
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
extern s32 osPfsReadWriteFile(OSPfs *, s32, u8, int, int, u8 *);

typedef struct C96File C96File;
typedef struct C96Handle { C96File *file; } C96Handle;
struct C96File {
    C96Handle *next;
    C96Handle *prev;    /* PC: paksys' tPS_File, 96 bytes (docs/port/POINTERS.md 9.10) */
    void (*RemoveFile)(C96Handle *);
    void (*MoveFile)(C96Handle *, C96Handle *);
    u8 channel, slot;
    u8 unknown12[46];
    u32 data_size;
    u8 unknown44[4];
    u8 **data;
};
typedef struct C96Slot {
    u8 unknown0;
    s8 modified;
    s8 flag2;
    u8 unknown3;
    u16 map_bytes;
    u16 unknown6;
    u32 allocation;
    u8 unknown12[28];
} C96Slot;
typedef struct C96Channel {
    u8 unknown0;
    s8 state;
    u8 unknown2[4];
    s8 flag6, busy, result, flag9;
    u8 unknown10;
    s8 modified;
    OSPfs pfs;
    s32 error;
    u8 unknown120[12];
    C96Slot slots[16];
} C96Channel;
/* Each pak's open files (paksysdc's tDLList, MAT3 tDLList): head the first. */
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    u32 count;
    C96Handle *head, *tail;
} tDLList;
extern volatile C96Channel gPaks[];
extern tDLList gPS_Files[];
extern OSMesgQueue si_lock_queue;
extern OSMesg si_lock_msg;
extern s8 si_lock_inited, gErrPak;
extern void (*gErrorFunc)(s32,s32,s32,s32,s8 *,s8 *,s32);
extern s32 pak_error_from_pfs(s32);
void pfs_queue_init(void) {
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue, &si_lock_msg, 1);
        osSendMesg(&si_lock_queue, 0, 0);
    }
}
void pfs_lock(void) {
    OSMesg message;
    pfs_queue_init();
    osRecvMesg(&si_lock_queue, &message, 1);
}
void pfs_unlock(void) {
    osSendMesg(&si_lock_queue, 0, 0);
}
void ClearFlush(s32 index) {
    C96Handle *p, *next;
    C96File *data;

    if (gPaks[index].modified == 0) { next = p = 0; goto out; }
    next = p = gPS_Files[index].head;
    while (p != 0) {
        data = p->file;
        next = data->next;
        if (data->data != 0) {
            if (gPaks[index].slots[data->slot].modified != 0) { next = 0; break; }
        }
        p = next;
    }
    if (p == 0) gPaks[index].modified = 0;
out:
    if (next) {}
}
s32 pak_find_dirty_run(C96Handle *handle, u8 **address, s32 *bytes, s32 *offset) {
    u8 complete = 0;
    C96File *file = handle->file;
    u8 *map;
    u32 i, j;
    if (!file->data) return 0;
    *bytes = 0;
    map = *file->data + file->data_size;
    for (i = 0; i < gPaks[file->channel].slots[file->slot].map_bytes; i++) {
        if (map[i] != 0) {
        for (j = 0; j < 8; j++) {
            if (map[i] & (1 << j)) {
                if (*bytes == 0) {
                    *offset = (i * 8 + j) * 32;
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
    *bytes *= 32;
    return (*bytes == 0) ^ 1;
}
void pak_mark_dirty(C96Handle *handle, u8 *address, u32 bytes, s32 mark) {
    C96File *file = handle->file;
    u8 *map;
    u32 first, last, i;
    u8 *end;
    if (address < *file->data || address >= *file->data + file->data_size) {
    }
    end = address + bytes;
    bytes = (u32)(address - *file->data);
    first = bytes;
    bytes >>= 5;
    last = (u32)(end - *file->data - 1) >> 5;
    map = *file->data + file->data_size;
    for (i = bytes; i <= last; i++) {
        if (mark) map[i >> 3] |= 1 << (i & 7);
        else map[i >> 3] &= ~(1 << (i & 7));
    }
    if (first) {}
}
s32 PS_FlushFileInternal(C96Handle *handle, s32 mode) {
    s32 result;
    s32 index, slot_index;
    s8 retry, callback_value;
    u8 *address;
    s32 bytes, offset;
    u8 **allocation;
    if (!handle) return 1;
    index = handle->file->channel;
    slot_index = handle->file->slot;
    allocation = handle->file->data;
    if (!gPaks[index].slots[slot_index].modified) return 1;
    osWritebackDCache(*allocation, handle->file->data_size);
    if (pak_find_dirty_run(handle, &address, &bytes, &offset)) {
        do {
            osWritebackDCache(address, bytes);
write_retry:
            result = osPfsReadWriteFile(&gPaks[index].pfs, slot_index, 1, offset, bytes, address);
            if (result) {
                gPaks[index].error = pak_error_from_pfs(result);
                if (!mode) {
                    if (gPaks[index].flag6) gPaks[index].error = 3;
                    else if (gPaks[index].state && !gPaks[index].flag9) gPaks[index].error = 3;
                    else if (!gPaks[index].state) gPaks[index].error = 2;
                    pfs_unlock();
                    gErrPak = index;
                    gErrorFunc(index, gPaks[index].error, 0, 0, &retry, &callback_value, 0);
                    gErrPak = -1;
                    pfs_lock();
                    if (retry) goto write_retry;
                }
                if (!mode) {
                    gPaks[index].slots[slot_index].modified = 0;
                    ClearFlush(index);
                }
                return 0;
            }
            pak_mark_dirty(handle, address, bytes, 0);
        } while (pak_find_dirty_run(handle, &address, &bytes, &offset));
    }
    gPaks[index].slots[slot_index].modified = 0;
    ClearFlush(index);
    return 1;
}
s32 PS_FlushPak(s32 index, s32 mode) {
    C96Handle *node;
    C96Handle *next;
    gPaks[index].result = 0;
    if (!gPaks[index].modified) return 1;
    pfs_lock();
    gPaks[index].busy = 1;
    node = gPS_Files[index].head;
    while (node) {
        next = node->file->next;
        if (node->file->data && gPaks[index].slots[node->file->slot].modified) {
            if (!PS_FlushFileInternal(node, mode)) break;
            gPaks[index].result = 1;
        }
        node = next;
    }
    gPaks[index].busy = 0;
    pfs_unlock();
    return node == 0;
}
s32 PS_FlushFile(C96Handle *handle, s32 mode) {
    pfs_lock();
    gPaks[handle->file->channel].busy = 1;
    gPaks[handle->file->channel].result = 0;
    gPaks[handle->file->channel].result = PS_FlushFileInternal(handle, mode);
    gPaks[handle->file->channel].busy = 0;
    pfs_unlock();
    return gPaks[handle->file->channel].result;
}
void PS_WriteFile(C96Handle *handle, u8 *address, u32 bytes) {
    C96File *file;
    if (!handle) return;
    file = handle->file;
    if (gPaks[file->channel].slots[file->slot].flag2) {}
    pak_mark_dirty(handle, address, bytes, 1);
    gPaks[file->channel].slots[file->slot].modified = 1;
    gPaks[file->channel].modified = 1;
}
