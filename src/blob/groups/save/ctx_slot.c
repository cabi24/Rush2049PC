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
#define N64W(x) PC_N64W(x)

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
 * unzipdata and CalcChecksumInternal take the pointer at it, N64PTR0(T, a) (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
s32 unzipdata(void *, s32, void *);

#define N64PTR0(T, a) ((T)PC_N64P(a))
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

/* PS_PurgeFile (historical label): release a slot's data block and clear its seat flag. */
/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void PS_PurgeFile(Handle h) {
    Slot *s = *(Slot **)h;

    if (s->data != NULL) {
        void **data = s->data;
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(N64A(data), 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        s->data = NULL;
        gPaks[s->id].seat[s->seat].busy = 0;
    }
}

void PurgeGhostData(Handle h) {
    Obj *o = *h;

    if (o->h36 != NULL) {
        void **old = o->h36;
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(N64A(old), 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        o->h36 = NULL;
        ((START_VIEW *)*o->h40)->f76 = 0;
        if (o->slot != NULL) {
            PS_PurgeFile(o->slot);
            o->h40 = NULL;
        }
    }
}

void DecodeGhost(Handle h) {
    MenuData *m = *(MenuData **)(*h)->h40;
    s32 n;

    if (m->buffer == 0) {
        (*h)->h36 = (void **)AllocateHandleHeap(NULL, m->tag);
        m->buffer = SLOT_S32((*h)->h36);
        n = unzipdata(m->data, m->size, N64PTR0(void *, m->buffer));
        m->count = n / 3;
        do {} while (0); /* f1: compiled-out block end (disclosed shaping): retail colours n into a1 */
        CalcChecksumInternal(N64PTR0(void *, m->buffer), n);
        m->loaded = 1;
    }
}

/* GC DeallocateMemory as used by SaveGhost's main path: heap lock/unlock as nested inlined helpers (frame
 * evidence: the inlined `address` slot sits at retail's 80(sp) only with the two nested 8 B areas below it, as
 * PS_DeleteFile's pak_buffer_free). Non-static, internal (not on keep). */
void ghost_heap_lock(void) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
}
void ghost_heap_unlock(void) {
    osSendMesg(&heap_lock_queue, NULL, 0);
}
void cart_free(void **address) {
    ghost_heap_lock();
    DeallocateMemoryInternal(N64A(address), 0);
    ghost_heap_unlock();
}

void slot_forget(Handle node) {
    s32 i;
    for (i = 0; i < 4; i++) {
        if (gGhosts[i] == node) {
            gGhosts[i] = NULL;
            break;
        }
    }
}

/* SaveGhost's limbo-path free: the same heap free written without nested helpers (one 8 B inline area; frame
 * evidence). Identity hypothesis: disclosed. */
void cart_free_direct(void **address) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(N64A(address), 0);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
void DeleteGhost(Handle ghosth) {   /* GC save.c DeleteGhost */
    if ((*ghosth)->slot != NULL) {
        PS_DeleteFile((*ghosth)->slot);
    }
}

/* "BCK"/"FWD" for the ghost file extension (GC SaveGhost picks it inline next to GetGhostExtension); an inlined
 * helper taking the header pointer (8 B inline area + retail's temp-ring register for the byte load). */
char *GhostDirName(MenuData *header) {
    if (header->variant) {
        return ghost_backward_suffix;
    }
    return ghost_forward_suffix;
}

/* func_800CC040 = GC save.c SaveGhost(pak, saveGhosth, limbo), N64 form (pak, player, saveGhosth, limbo); member
 * since (EQUAL in the whole-program unit, MATCH in the group compile). Parameter names kept from the draft:
 * id = pak, owner = player handle, self = saveGhosth, release = limbo; m = header, node = before, buf = str,
 * obj = ghost, created = fileh. GC calls: SaveGhostInLimbo, DeallocateMemory = cart_free /
 * cart_free_direct, _FreeGhost = FreeGhost, _ReadGhostData = ReadGhostDataInternal, DeleteGhost, GetPtrSize =
 * GetPtrSize, InsertLLE, gGhostFiles = gGhostFiles. */
s32 SaveGhost(s32 id, Handle owner, Handle self, s32 release) {
    MenuData *m;
    Handle node;
    char buf[32];
    Obj *obj;
    Handle created;
    u32 size;

    m = *(MenuData **)(*self)->h40;
    if (release) {
        SaveGhostInLimbo(m, m->data);
        cart_free_direct((*self)->h40);
        FreeGhost(self);
        return 1;
    }
    for (node = gGhostFiles.head; node != NULL; node = obj->next) {
        obj = *node;
        if (id == ((Slot *)*obj->slot)->id && game_strcmp(obj->name, m->name) == 0 &&
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
    obj = *self;
    size = (u32)GetPtrSize(N64A(obj->h40));
    sprintf(buf, ghost_file_name_format, (char *)*M2C_FIELD(*owner, void ***, PROFILE_OFS(8)) + PSF_OFS(18), m->index + 1,
            GhostDirName(m));
    created = PS_CreateFile(id, buf, ghost_file_ext, size, 0x3544, 0x4E525545);
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
    PS_FlushPak(id, 1);
    PS_PurgeFile(created);
    obj->h40 = NULL;
    obj->h36 = NULL;
    for (node = gGhostFiles.head; node != NULL; node = (*node)->next) {
        if (obj->order < (*node)->order) {
            break;
        }
    }
    InsertLLE(&gGhostFiles, self, node);
    return 1;
}

s32 ReadGhostData(Handle h) {
    return ReadGhostDataInternal(h, 1, 1);
}
