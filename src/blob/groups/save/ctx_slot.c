/* Ghost files: SaveGhost, reading, decoding and dropping a ghost's data, PS_PurgeFile (MAT3 save.c and ghost.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

#undef M2C_FIELD
#define M2C_FIELD(expr,type,offset) (*(type)((s8 *)(expr)+(offset)))
typedef struct OSMesgQueue OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *,void **,s32);s32 osSendMesg(OSMesgQueue *,void *,s32);

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

/* ---- heap release (copied from src/blob/groups/heap_release, matched there) ---- */
enum n64p_Block { n64p_Block_ };
enum n64p_Pool { n64p_Pool_ };
enum n64p_u32 { n64p_u32_ };
typedef struct Block {u32 pad0;enum n64p_Block next;u32 pad8[3];s8 pad14; s8 tag; u8 counter;u8 pad23[9];} Block;
typedef struct Pool {s32 count;enum n64p_u32 base;enum n64p_Pool next;} Pool;

/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Pool: (struct Pool *)PC_N64P(x), enum n64p_u32: (u32 *)PC_N64P(x), default: (x))

/* ---- menu objects ---- */
typedef struct MenuData {
    u8 pad0[5];
    s8 loaded;        /* 5 */
    s8 index;         /* 6 */
    u8 variant;       /* 7 */
    u8 pad8[14];
    char name[18];    /* 22 */
    s32 key0;         /* 40 */
    s32 key1;         /* 44 */
    u8 pad48[12];
    s32 size;         /* 60 */
    s32 tag;          /* 64 */
    u8 pad68[8];
    s32 buffer;       /* 76 */
    u8 pad80[4];
    s32 count;        /* 84 */
    u8 data[4];       /* 88 */
} MenuData;
typedef struct Obj Obj;
typedef Obj **Handle;
struct Obj {
    Handle next;      /* 0 */
    Handle slot;      /* 4 */
    s8 index;         /* 8 */
    u8 variant;       /* 9 */
    char name[14];    /* 10 */
    s32 key0;         /* 24 */
    s32 key1;         /* 28 */
    f32 order;        /* 32 */
    void **h36;       /* 36 */
    void **h40;       /* 40 */
};
/* PC: paksys' tPS_File (96 bytes, docs/port/POINTERS.md 9.10). PurgeGhostData also reaches the ghost header's
 * start (N64 0x4C, 32 bits in the file data) through this view, as f76: on the PC through HeaderStart. */
typedef struct Slot {
    void *next, *prev;
    void *update;     /* RemoveFile */
    void *render;     /* MoveFile */
    u8 id;
    u8 seat;
    u8 pad18[46];
    s32 catchup;
    u8 pad68[4];
    void **data;
} Slot;
typedef struct HeaderStart { u8 pad0[76]; s32 f76; } HeaderStart;
#define START_VIEW HeaderStart
typedef struct List { s32 pad0; s32 pad4; Handle head, tail; } List;

VIEW(List, gGhostFiles);
#define gGhostFiles gGhostFiles_view
VIEW(Handle[4], gGhosts);
#define gGhosts gGhosts_view

s32 sprintf(char *, const char *, ...);
void *memcpy(void *, const void *, u32);

Handle PS_CreateFile(s32, char *, char *, u32, s32, s32);
void PS_WriteFile(void *, void *, u32);

/* The ghost's buffer is kept as its 32-bit N64 address (MenuData.buffer), read from its handle's slot (PC_SLOT_N64);
 * unzipdata and CalcChecksumInternal take the pointer at it, (T)PC_N64P(a) (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
s32 unzipdata(void *, s32, void *);

#define SLOT_S32(h) (s32)PC_SLOT_N64(h)

/* The seats are the record's 16 file entries (0x28 bytes from +0x84 on the N64); the N64 view starts them 0x20
 * early, at +0x64, with busy at +0x22. */
typedef struct {
    u8 pad0[2];
    u8 busy;          /* file entry +2 */
    u8 pad3[37];
} SeatEntry;
typedef struct {
    u8 pad0[12];
    PcPfs pfs;
    s32 error, FreeSpace;
    u8 pad7C[8];
    SeatEntry seat[16];  /* the file entries */
} PlayerSeats;
VIEW(PlayerSeats[], gPaks);
#define gPaks gPaks_view

/* Frees an open pak file's data (it is read again when needed) and clears the file's lock flag. */
void PS_PurgeFile(Handle h) {
    Slot *s = *(Slot **)h;

    if (s->data != NULL) {
        void **data = s->data;
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(PC_N64A(data), 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        s->data = NULL;
        gPaks[s->id].seat[s->seat].busy = 0;
    }
}

/* Frees a ghost's decoded recording and its file's data (PS_PurgeFile), clearing the header's recording address. */
void PurgeGhostData(Handle h) {
    Obj *o = *h;

    if (o->h36 != NULL) {
        void **old = o->h36;
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(PC_N64A(old), 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        o->h36 = NULL;
        ((START_VIEW *)*o->h40)->f76 = 0;
        if (o->slot != NULL) {
            PS_PurgeFile(o->slot);
            o->h40 = NULL;
        }
    }
}

/* Decompresses a ghost's recording (unzipdata) into a new heap block, once: the header then holds the block's address
 * and the number of samples (three bytes each). */
void DecodeGhost(Handle h) {
    MenuData *m = *(MenuData **)(*h)->h40;
    s32 n;

    if (m->buffer == 0) {
        (*h)->h36 = (void **)AllocateHandleHeap(NULL, m->tag);
        m->buffer = SLOT_S32((*h)->h36);
        n = unzipdata(m->data, m->size, ((void *)PC_N64P(m->buffer)));
        m->count = n / 3;
        do {} while (0);
        CalcChecksumInternal(((void *)PC_N64P(m->buffer)), n);
        m->loaded = 1;
    }
}

/* The heap lock (MAT3's DeallocateMemory, inline in SaveGhost: take, free, give back). */
void ghost_heap_lock(void) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
}
/* Gives the heap lock back. */
void ghost_heap_unlock(void) {
    osSendMesg(&heap_lock_queue, NULL, 0);
}
/* Frees the heap block at address, under the heap lock. */
void cart_free(void **address) {
    ghost_heap_lock();
    DeallocateMemoryInternal(PC_N64A(address), 0);
    ghost_heap_unlock();
}

/* Forgets the ghost node if a player races against it (gGhosts). */
void slot_forget(Handle node) {
    s32 i;
    for (i = 0; i < 4; i++) {
        if (gGhosts[i] == node) {
            gGhosts[i] = NULL;
            break;
        }
    }
}

/* Frees the heap block at address, under the heap lock (cart_free written out). */
void cart_free_direct(void **address) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(PC_N64A(address), 0);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
/* Deletes the ghost's file from its pak, if it has one. */
void DeleteGhost(Handle ghosth) {
    if ((*ghosth)->slot != NULL) {
        PS_DeleteFile((*ghosth)->slot);
    }
}

/* The ghost file name's direction suffix: backward or forward, by the header. */
char *GhostDirName(MenuData *header) {
    if (header->variant) {
        return ghost_backward_suffix;
    }
    return ghost_forward_suffix;
}

/* Saves the new ghost saveGhosth of player on pak's pak (MAT3 SaveGhost(pak, saveGhosth, limbo); the N64 adds the
 * player). With limbo it is only kept in memory (SaveGhostInLimbo) and freed. Else an older ghost of the same
 * driver, track and direction on that pak goes to limbo and is deleted; a file named after the player's file, the
 * track and the direction is created, the ghost written to it and its data dropped from memory, and the ghost joins
 * the ghost list in order of its time. Returns 0 when the file can't be created. */
s32 SaveGhost(s32 pak, Handle player, Handle saveGhosth, s32 limbo) {
    MenuData *m;
    Handle node;
    char buf[32];
    Obj *obj;
    Handle created;
    u32 size;

    m = *(MenuData **)(*saveGhosth)->h40;
    if (limbo) {
        SaveGhostInLimbo(m, m->data);
        cart_free_direct((*saveGhosth)->h40);
        FreeGhost(saveGhosth);
        return 1;
    }
    for (node = gGhostFiles.head; node != NULL; node = obj->next) {
        obj = *node;
        if (pak == ((Slot *)*obj->slot)->id && game_strcmp(obj->name, m->name) == 0 &&
            m->key0 == obj->key0 && m->key1 == obj->key1 &&
            obj->index == m->index && obj->variant == m->variant) {
            slot_forget(node);
            if (ReadGhostDataInternal(node, 0, 0)) {
                SaveGhostInLimbo(*obj->h40, ((MenuData *)*obj->h40)->data);
                DeleteGhost(node);
            }
            break;
        }
    }
    obj = *saveGhosth;
    size = (u32)GetPtrSize(PC_N64A(obj->h40));
    sprintf(buf, ghost_file_name_format, (char *)*M2C_FIELD(*player, void ***, PROFILE_OFS(8)) + PSF_OFS(18), m->index + 1,
            GhostDirName(m));
    created = PS_CreateFile(pak, buf, ghost_file_ext, size, GAME_COMPANY_CODE, GAME_CODE);
    if (created == NULL) {
        return 0;
    }
    obj->slot = created;
    ((Slot *)*created)->update = RemovePlayerFile;
    ((Slot *)*obj->slot)->render = MovePlayerFile;
    memcpy(*((Slot *)*created)->data, *obj->h40, size);
    cart_free(obj->h40);
    obj->h40 = ((Slot *)*created)->data;
    PS_WriteFile(created, *obj->h40, size);
    PS_FlushFile(created, 0);
    PS_FlushPak(pak, 1);
    PS_PurgeFile(created);
    obj->h40 = NULL;
    obj->h36 = NULL;
    for (node = gGhostFiles.head; node != NULL; node = (*node)->next) {
        if (obj->order < (*node)->order) {
            break;
        }
    }
    InsertLLE(&gGhostFiles, saveGhosth, node);
    return 1;
}

/* Reads the ghost's file, checks it, decodes its recording and moves both low in the heap (ReadGhostDataInternal). */
s32 ReadGhostData(Handle h) {
    return ReadGhostDataInternal(h, 1, 1);
}
