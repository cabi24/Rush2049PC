/* Context copy of src/blob/groups/menu_cc040_20261004/group.c (slot_release) with its func_800CBF2C context draft
 * removed (the real body is in group.c). Callers of func_800CBF2C (func_800CC040, menu_item_value_get) and its IPA
 * callee menu_back (s1) must be in this group's program for func_800CBF2C's register convention. */
typedef signed char s8;typedef unsigned char u8;typedef short s16;typedef unsigned short u16;
typedef signed int s32;typedef unsigned int u32;typedef float f32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
#define NULL ((void *)0)
#define M2C_FIELD(expr,type,offset) (*(type)((s8 *)(expr)+(offset)))
typedef struct OSMesgQueue OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *,void **,s32);s32 osSendMesg(OSMesgQueue *,void *,s32);
extern intptr_t D_801527C8;
extern OSMesgQueue D_80152770;

/* ---- heap release (copied from src/blob/groups/heap_release, matched there) ---- */
#ifdef TARGET_PC
enum n64p_Block { n64p_Block_ };
enum n64p_Pool { n64p_Pool_ };
enum n64p_u32 { n64p_u32_ };
#endif
#ifdef TARGET_PC
typedef struct Block {u32 pad0;enum n64p_Block next;u32 pad8[3];s8 pad14; s8 tag; u8 counter;u8 pad23[9];} Block;
#else
typedef struct Block {u32 pad0;struct Block *next;u32 pad8[3];s8 pad14; s8 tag; u8 counter;u8 pad23[9];} Block;
#endif
#ifdef TARGET_PC
typedef struct Pool {s32 count;enum n64p_u32 base;enum n64p_Pool next;} Pool;
#else
typedef struct Pool {s32 count;u32 *base;struct Pool *next;} Pool;
#endif
#ifdef TARGET_PC
typedef struct Heap {u32 pad0;u32 pad4;enum n64p_Block blocks;u32 padc[3];Pool pool;} Heap;
#else
typedef struct Heap {u32 pad0;u32 pad4;Block *blocks;u32 padc[3];Pool pool;} Heap;
#endif
#ifdef TARGET_PC
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)(uintptr_t)(x), enum n64p_Pool: (struct Pool *)(uintptr_t)(x), enum n64p_u32: (u32 *)(uintptr_t)(x), default: (x))
#define N64W(x) PC_N64W(x)
#else
#define N64P(x) x
#define N64W(x) x
#endif
void func_80095EC0(void *,u32);
void *func_80095F8C(u32 arg0) {
    s32 var_v0;
    s32 var_v1;

    var_v0 = D_801527C8;
    var_v1 = 0;
    if (var_v0 != 0) {
        do {
            if ((arg0 >= (u32) M2C_FIELD(var_v0, u32 *, 8)) && (arg0 < (u32) M2C_FIELD(var_v0, u32 *, 0x10))) {
                var_v1 = var_v0;
            }
            var_v0 = M2C_FIELD(var_v0, s32 *, 4);
        } while (var_v0 != 0);
    }
    return (void *) var_v1;
}
Block *func_80095EF4(Heap *heap, u32 addr, s32 tag) {
    Pool *p;
    Block *b;
    p = &heap->pool;
    while (1) {
        if (p == 0 || p->base == 0) break;
        if (addr >= (u32)p->base && addr < (u32)(N64P(p->base) + p->count)) {
            addr = *(u32 *)addr;
            break;
        }
        p = N64P(p->next);
    }
    b = N64P(heap->blocks);
    while (b != 0) {
        if (tag != b->tag || addr < (u32)b || (b->next != 0 && addr >= (u32)b->next)) {
            b = N64P(b->next);
        } else break;
    }
    return b;
}
void audio_reverb_update(u32 address,s32 tag) {
 Heap *heap; Block *block,*next,*prev;
 heap=func_80095F8C(address);
 block=func_80095EF4(heap,address,tag);
 func_80095EC0((u8 *)block+32,block->pad8[1]);
 if(block->pad8[2]) *(u32 *)block->pad8[2]=0;
 next=N64P(block->next);
 if(next && next->pad14==0) {
  block->next=next->next;
  if(block->next) N64P(block->next)->pad8[0]=(u32)block;
  else heap->padc[0]=(u32)block;
  block->pad8[1]+=next->pad8[1]+32;
  func_80095EC0(next,32);
 }
 prev=(Block *)block->pad8[0];
 block->pad8[2]=0;block->pad14=0;block->tag=0;block->counter=0;
 if(prev && prev->pad14==0) {
  prev->next=block->next;
  if(prev->next) N64P(prev->next)->pad8[0]=(u32)prev;
  else heap->padc[0]=(u32)prev;
  prev->pad8[1]+=block->pad8[1]+32;
  func_80095EC0(block,32);
 }
}

void *audio_buffer_sync(u32 address) {
 void *owner,*block,*result;
 osRecvMesg((OSMesgQueue *)&D_80152770,NULL,1);
 owner=func_80095F8C(address);
 block=func_80095EF4(owner,address,0);
 result=M2C_FIELD(block,void **,12);
 osSendMesg((OSMesgQueue *)&D_80152770,NULL,0);
 return result;
}

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
#ifdef TARGET_PC
/* PC: paksys' tPS_File (96 bytes, docs/port/POINTERS.md 9.10). menu_transition also reaches the ghost header's
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
#else
typedef struct Slot {
    u8 pad0[8];
    void *update;     /* 8 */
    void *render;     /* 12 */
    u8 id;            /* 16 */
    u8 seat;          /* 17 */
    u8 pad18[46];
    s32 catchup;      /* 64 */
    u8 pad68[4];
    void **data;      /* 72 */
    s32 f76;          /* 76 */
} Slot;
#define START_VIEW Slot
#define PROFILE_OFS(n) n
#define PSF_OFS(n) n
typedef struct List { s32 pad0; s32 pad4; Handle head; } List;
#endif

extern List D_80152020;
extern Handle D_80152698[4];
extern char D_801211E4[], D_801211EC[], D_801211F0[], D_801211F4[];
void audio_volume_pan(void);
void func_800949D4(void);
s32 sprintf(char *, const char *, ...);
void *memcpy(void *, const void *, u32);
s32 func_8008AD04(char *, char *);
void func_800CB748(void *, void *);
void pak_delete_file_retry(void *);
Handle PS_CreateFile(s32, char *, char *, u32, s32, s32);
void PS_WriteFile(void *, void *, u32);
s32 PS_FlushFile(void *, s32);
s32 PS_FlushPak(s32, s32);
void func_80091FBC(List *, Handle, Handle);
void drone_set_catchup(void *, s32, s32);
#ifdef TARGET_PC
void *AllocateHandleHeap(void *, s32);
#else
s32 AllocateHandleHeap(void *, s32);
#endif
s32 inflate_entry_alt(void *, s32, s32);
s32 format_string_parse(s32, s32);
s32 draw_ui_element(Handle, s32, s32);
void menu_item_select(void *, s32);
void func_800CB9D0(void *);

#ifdef TARGET_PC
/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
/* The seats are the record's 16 file entries (0x28 bytes from +0x84 on the N64); the N64 view starts them 0x20
 * early, at +0x64, with busy at +0x22. */
typedef struct {
    u8 pad0[2];
    u8 busy;          /* file entry +2 */
    u8 pad3[37];
} SeatEntry;
#else
typedef struct {
    u8 pad0[0x22];
    u8 busy;          /* 0x22 */
    u8 pad23[5];
} SeatEntry;
#endif          /* 0x28 */
#ifdef TARGET_PC
typedef struct {
    u8 pad0[12];
    PcPfs pfs;
    s32 error, FreeSpace;
    u8 pad7C[8];
    SeatEntry seat[16];  /* the file entries */
} PlayerSeats;
#else
typedef struct {
    u8 pad0[0x64];
    SeatEntry seat[16];  /* 0x64 */
    u8 pad2E4[0x304 - 0x2E4];
} PlayerSeats;
#endif        /* 0x304, D_80144030 */
extern PlayerSeats D_80144030[];

/* AdjustSteer (historical label): release a slot's data block and clear its seat flag. */
void AdjustSteer(Handle h) {
    Slot *s = *(Slot **)h;

    if (s->data != NULL) {
        void **data = s->data;
        osRecvMesg(&D_80152770, NULL, 1);
        audio_reverb_update((u32)data, 0);
        osSendMesg(&D_80152770, NULL, 0);
        s->data = NULL;
        D_80144030[s->id].seat[s->seat].busy = 0;
    }
}

void menu_transition(Handle h) {
    Obj *o = *h;

    if (o->h36 != NULL) {
        void **old = o->h36;
        osRecvMesg(&D_80152770, NULL, 1);
        audio_reverb_update((u32)old, 0);
        osSendMesg(&D_80152770, NULL, 0);
        o->h36 = NULL;
        ((START_VIEW *)*o->h40)->f76 = 0;
        if (o->slot != NULL) {
            AdjustSteer(o->slot);
            o->h40 = NULL;
        }
    }
}

void DecodeGhost(Handle h) {
    MenuData *m = *(MenuData **)(*h)->h40;
    s32 n;

    if (m->buffer == 0) {
        (*h)->h36 = (void **)AllocateHandleHeap(NULL, m->tag);
        m->buffer = *(s32 *)(*h)->h36;
        n = inflate_entry_alt(m->data, m->size, m->buffer);
        m->count = n / 3;
        do {} while (0); /* f1: compiled-out block end (disclosed shaping): retail colours n into a1 */
        format_string_parse(m->buffer, n);
        m->loaded = 1;
    }
}

s32 func_800CBF2C(Handle h, s32 back, s32 select);  /* defined in group.c */

/* GC DeallocateMemory as used by SaveGhost's main path: heap lock/unlock as nested inlined helpers (frame
 * evidence: the inlined `address` slot sits at retail's 80(sp) only with the two nested 8 B areas below it, as
 * pak_delete_file_retry's pak_buffer_free). Non-static, internal (not on keep). */
void ghost_heap_lock(void) {
    osRecvMesg(&D_80152770, NULL, 1);
}
void ghost_heap_unlock(void) {
    osSendMesg(&D_80152770, NULL, 0);
}
void cart_free(void **address) {
    ghost_heap_lock();
    audio_reverb_update((u32)address, 0);
    ghost_heap_unlock();
}

void slot_forget(Handle node) {
    s32 i;
    for (i = 0; i < 4; i++) {
        if (D_80152698[i] == node) {
            D_80152698[i] = NULL;
            break;
        }
    }
}

void func_800C7300(Handle ghosth);   /* GC _FreeGhost (group.c member, stub 0x800C7300) */
/* SaveGhost's limbo-path free: the same heap free written without nested helpers (one 8 B inline area; frame
 * evidence). Identity hypothesis: disclosed. */
void cart_free_direct(void **address) {
    osRecvMesg(&D_80152770, NULL, 1);
    audio_reverb_update((u32)address, 0);
    osSendMesg(&D_80152770, NULL, 0);
}
void DeleteGhost(Handle ghosth) {   /* GC save.c DeleteGhost */
    if ((*ghosth)->slot != NULL) {
        pak_delete_file_retry((*ghosth)->slot);
    }
}

/* "BCK"/"FWD" for the ghost file extension (GC SaveGhost picks it inline next to GetGhostExtension); an inlined
 * helper taking the header pointer (8 B inline area + retail's temp-ring register for the byte load). */
char *GhostDirName(MenuData *header) {
    if (header->variant) {
        return D_801211EC;
    }
    return D_801211F0;
}

/* func_800CC040 = GC save.c SaveGhost(pak, saveGhosth, limbo), N64 form (pak, player, saveGhosth, limbo); member
 * since (EQUAL in the whole-program unit, MATCH in the group compile). Parameter names kept from the draft:
 * id = pak, owner = player handle, self = saveGhosth, release = limbo; m = header, node = before, buf = str,
 * obj = ghost, created = fileh. GC calls: SaveGhostInLimbo = func_800CB748, DeallocateMemory = cart_free /
 * cart_free_direct, _FreeGhost = func_800C7300, _ReadGhostData = func_800CBF2C, DeleteGhost, GetPtrSize =
 * audio_buffer_sync, InsertLLE = func_80091FBC, gGhostFiles = D_80152020. */
s32 SaveGhost(s32 id, Handle owner, Handle self, s32 release) {
    MenuData *m;
    Handle node;
    char buf[32];
    Obj *obj;
    Handle created;
    u32 size;

    m = *(MenuData **)(*self)->h40;
    if (release) {
        func_800CB748(m, m->data);
        cart_free_direct((*self)->h40);
        func_800C7300(self);
        return 1;
    }
    for (node = D_80152020.head; node != NULL; node = obj->next) {
        obj = *node;
        if (id == ((Slot *)*obj->slot)->id && func_8008AD04(obj->name, m->name) == 0 &&
            m->key0 == obj->key0 && m->key1 == obj->key1 &&
            obj->index == m->index && obj->variant == m->variant) {
            slot_forget(node);
            if (func_800CBF2C(node, 0, 0)) {
                func_800CB748(*obj->h40, ((MenuData *)*obj->h40)->data);
                DeleteGhost(node);
            }
            break;
        }
    }
    obj = *self;
    size = (u32)audio_buffer_sync((u32)obj->h40);
    sprintf(buf, D_801211E4, (char *)*M2C_FIELD(*owner, void ***, PROFILE_OFS(8)) + PSF_OFS(18), m->index + 1,
            GhostDirName(m));
    created = PS_CreateFile(id, buf, D_801211F4, size, 0x3544, 0x4E525545);
    if (created == NULL) {
        return 0;
    }
    obj->slot = created;
    ((Slot *)*created)->update = audio_volume_pan;
    ((Slot *)*obj->slot)->render = func_800949D4;
    memcpy(*((Slot *)*created)->data, *obj->h40, size);
    cart_free(obj->h40);
    obj->h40 = ((Slot *)*created)->data;
    PS_WriteFile(created, *obj->h40, size);
    PS_FlushFile(created, 0);
    PS_FlushPak(id, 1);
    AdjustSteer(created);
    obj->h40 = NULL;
    obj->h36 = NULL;
    for (node = D_80152020.head; node != NULL; node = (*node)->next) {
        if (obj->order < (*node)->order) {
            break;
        }
    }
    func_80091FBC(&D_80152020, self, node);
    return 1;
}

s32 menu_item_value_get(Handle h) {
    return func_800CBF2C(h, 1, 1);
}
