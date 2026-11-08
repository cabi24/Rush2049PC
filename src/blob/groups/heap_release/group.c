typedef signed char s8;typedef unsigned char u8;typedef signed int s32;typedef unsigned int u32;
#define NULL ((void *)0)
#define M2C_FIELD(expr,type,offset) (*(type)((s8 *)(expr)+(offset)))
enum n64p_Block { n64p_Block_ };
enum n64p_Pool { n64p_Pool_ };
enum n64p_u32 { n64p_u32_ };
enum n64p_MBlock { n64p_MBlock_ };
enum n64p_void { n64p_void_ };
typedef struct Block {u32 pad0;enum n64p_Block next;u32 pad8[3];s8 pad14; s8 tag; u8 counter;u8 pad23[9];} Block;
typedef struct Pool {s32 count;enum n64p_u32 base;enum n64p_Pool next;} Pool;
typedef struct Heap {u32 pad0;u32 pad4;enum n64p_Block blocks;u32 padc[3];Pool pool;} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Pool: (struct Pool *)PC_N64P(x), enum n64p_u32: (u32 *)PC_N64P(x), enum n64p_MBlock: (struct MBlock *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
/* The heap's links and addresses are 32-bit N64 addresses: N64A(p) is a window pointer's, N64PTR(T, a) the pointer
 * at one (pc_prelude.h, docs/port/NOFIXED.md 2.3). A block's owner is the handle slot that holds it
 * (AllocateHandleHeap), cleared as a slot (PC_SLOT_SET). */
#define N64A(x) PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))
#define SLOT_CLEAR(o) PC_SLOT_SET(PC_N64P(o), 0)
typedef struct OSMesgQueue OSMesgQueue;
extern intptr_t gMainHeap;
extern s32 heap_lock_queue;
extern s8 extra_overlay_loaded,start_overlay_loaded;
extern u8 overlay_base[];
typedef struct RaceEntry {u32 allocation;u8 pad4[20];} RaceEntry;
extern RaceEntry car_palettes_palBase[];
s32 osRecvMesg(OSMesgQueue *,void **,s32);s32 osSendMesg(OSMesgQueue *,void *,s32);
void FillFreeMem(void *,u32);
void *FindHeap(u32 arg0);
Block *FindChunk(Heap *heap, u32 addr, s32 tag);
/* PC: the free's first step poisons the freed block (block + 32, size). Whatever was registered big-endian in
 * it (a wave's file) goes with it (port/assets). The other two calls clear merged headers. */
void pc_heap_block_freed(void *p, __SIZE_TYPE__ size);
#define FillFreeMem(p, n) (pc_heap_block_freed(p, n), FillFreeMem(p, n))
void DeallocateMemoryInternal(u32 address,s32 tag) {
 Heap *heap; Block *block,*next,*prev;
 heap=FindHeap(address);
 block=FindChunk(heap,address,tag);
 FillFreeMem((u8 *)block+32,block->pad8[1]);
 if(block->pad8[2]) SLOT_CLEAR(block->pad8[2]);
 next=N64P(block->next);
 if(next && next->pad14==0) {
  block->next=next->next;
  if(block->next) N64P(block->next)->pad8[0]=N64A(block);
  else heap->padc[0]=N64A(block);
  block->pad8[1]+=next->pad8[1]+32;
  FillFreeMem(next,32);
 }
 prev=N64PTR(Block *, block->pad8[0]);
 block->pad8[2]=0;block->pad14=0;block->tag=0;block->counter=0;
 if(prev && prev->pad14==0) {
  prev->next=block->next;
  if(prev->next) N64P(prev->next)->pad8[0]=N64A(prev);
  else heap->padc[0]=N64A(prev);
  prev->pad8[1]+=block->pad8[1]+32;
  FillFreeMem(block,32);
 }
}
#undef FillFreeMem
/* : __inline + four unused locals: umerge inlines it into AudFree3D below, and each inlined
 * instance reserves its callee's locals in the caller's frame (retail spill homes 56 / 32, frame 64). */
__inline void DeallocateMemory(u32 address) {
 s32 u0, u1, u2, u3;
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 DeallocateMemoryInternal(address,0);
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
}
void musyx_free(u32 address) {
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 DeallocateMemoryInternal(address,0);
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
}
void unload_extra_overlay(void) {
 if(extra_overlay_loaded) {
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 DeallocateMemoryInternal(N64A(overlay_base),0);
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
 extra_overlay_loaded=0;
 }
}
void unload_start_overlay(void) {
 if(start_overlay_loaded) {
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 DeallocateMemoryInternal(N64A(overlay_base),0);
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
 start_overlay_loaded=0;
 }
}
void free_car_palette(s32 index) {
 RaceEntry *entry=&car_palettes_palBase[index];
 u32 address=entry->allocation;
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 DeallocateMemoryInternal(address,0);
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
}

/*
 * ReallocateMemory (historical label, misleading): heap block resize in place.
 * (address, desired) -> takes the heap message-queue lock (heap_lock_queue), rounds
 * desired up to 32, finds the owning heap (FindHeap) and block header
 * (FindChunk, tag 0), then
 *   shrink, successor absent or in use: split off a free block when at least
 *     64 bytes remain (new size = old - desired - 32);
 *   shrink, successor free: when at least 32 bytes remain, move the
 *     successor's header down (new size = old - desired + successor);
 *   grow: successor is taken without a null/used test; when
 *     successor - desired + old < 32 it is absorbed whole (size = old +
 *     successor + 32), else its header moves up (old + successor - desired).
 * Block header: magic 0xFEDCBA98, next, prev, size, owner, used, tag, count.
 * Heap +0xC is the tail block. No arcade ancestor identified (N64 allocator).
 *
 * Whole-program member: FindHeap preserves a1/a3 and FindChunk
 * preserves a3/t0 for this caller, so it only reproduces with both real
 * lookups (internal) in the unit. Everything above this comment is the locked
 * heap_release group source, unchanged.
 *
 * Shaping notes: `block->next == NULL` / `moved->next = block->next` use the
 * field expression (v1) while `next` is its own variable (a1); the grow path
 * reads the successor size once into a local for the test only.
 */
/* PC: the heap's block and heap headers keep their N64 layout in the RDRAM window (alloc_at.c's Block and Heap,
 * docs/port/POINTERS.md 1): the links, the owner and the heap's last block are 32-bit window addresses. */
typedef struct MBlock {
    u32 magic;
    enum n64p_MBlock next;
    enum n64p_MBlock prev;
    u32 size;
    enum n64p_void owner;
    s8 used;
    s8 tag;
    u8 count;
} MBlock;
typedef struct MHeap {
    u32 pad0[3];
    enum n64p_MBlock tail;
} MHeap;

/* PC: a block that grows takes from the heap: reported as an allocation (port/ultra/heap.c). */

void ReallocateMemory(u32 address, u32 desired) {
    MHeap *heap;
    MBlock *block;
    MBlock *next;
    MBlock *moved;
    u32 size;
    u32 nsize;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    desired = (desired + 31) & ~31U;
    heap = (MHeap *)FindHeap(address);
    block = (MBlock *)FindChunk((Heap *)heap, address, 0);
    size = block->size;
    if (size >= desired) {
        next = N64P(block->next);
        if (N64P(block->next) == NULL || next->used != 0) {
            if (size - desired >= 64) {
                moved = (MBlock *)((u8 *)block + desired + 32);
                moved->next = block->next;
                if (N64P(moved->next) != NULL) {
                    N64P(moved->next)->prev = N64W(moved);
                } else {
                    heap->tail = N64W(moved);
                }
                moved->prev = N64W(block);
                moved->size = block->size - desired - 32;
                moved->owner = N64W(NULL);
                moved->used = 0;
                moved->tag = 0;
                moved->count = 0;
                moved->magic = 0xFEDCBA98;
                block->next = N64W(moved);
                block->size = desired;
            }
        } else {
            if (size - desired >= 32) {
                moved = (MBlock *)((u8 *)block + desired + 32);
                moved->next = next->next;
                if (N64P(moved->next) != NULL) {
                    N64P(moved->next)->prev = N64W(moved);
                } else {
                    heap->tail = N64W(moved);
                }
                moved->prev = N64W(block);
                moved->size = block->size - desired + next->size;
                moved->owner = N64W(NULL);
                moved->used = 0;
                moved->tag = 0;
                moved->count = 0;
                moved->magic = 0xFEDCBA98;
                block->next = N64W(moved);
                block->size = desired;
            }
        }
    } else {
        next = N64P(block->next);
        nsize = next->size;
        if (nsize - desired + size < 32) {
            block->next = next->next;
            if (N64P(block->next) != NULL) {
                N64P(block->next)->prev = N64W(block);
            } else {
                heap->tail = N64W(block);
            }
            desired = block->size + next->size + 32;
        } else {
            moved = (MBlock *)((u8 *)block + desired + 32);
            moved->next = next->next;
            if (N64P(moved->next) != NULL) {
                N64P(moved->next)->prev = N64W(moved);
            } else {
                heap->tail = N64W(moved);
            }
            moved->prev = N64W(block);
            moved->size = block->size + next->size - desired;
            moved->owner = N64W(NULL);
            moved->used = 0;
            moved->tag = 0;
            moved->count = 0;
            moved->magic = 0xFEDCBA98;
            block->next = N64W(moved);
        }
        block->size = desired;
    }
    pc_heap_allocated(heap); osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}

/* : AudFree3D -- frees the two cached audio heap handles gListeners / gEmitters (the first half
 * AudioShutdown's shutdown): for each non-zero handle, DeallocateMemory(handle) (inlined), then zero it.
 * DeallocateMemoryInternal is internal and clobbers s0/s1, which is why this caller saves them unused. */
extern uintptr_t gListeners;
extern uintptr_t gEmitters;

void AudFree3D(void) {
    if (gListeners) {
        DeallocateMemory(gListeners);
        gListeners = 0;
    }
    if (gEmitters) {
        DeallocateMemory(gEmitters);
        gEmitters = 0;
    }
}
