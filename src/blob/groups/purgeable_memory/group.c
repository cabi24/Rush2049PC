typedef signed char s8;typedef unsigned char u8;typedef signed int s32;typedef unsigned int u32;
#define NULL ((void *)0)
#define M2C_FIELD(expr,type,offset) (*(type)((s8 *)(expr)+(offset)))
enum n64p_Block { n64p_Block_ };
enum n64p_Pool { n64p_Pool_ };
enum n64p_u32 { n64p_u32_ };
typedef struct Block {u32 pad0;enum n64p_Block next;u32 pad8[3];s8 pad14; s8 tag; u8 counter;u8 pad23[9];} Block;
typedef struct Pool {s32 count;enum n64p_u32 base;enum n64p_Pool next;} Pool;
typedef struct Heap {u32 pad0;u32 pad4;enum n64p_Block blocks;u32 padc[3];Pool pool;} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Pool: (struct Pool *)PC_N64P(x), enum n64p_u32: (u32 *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
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
void PurgeableMemory(u32 address,s32 decrement) {
 void *owner,*block;s32 count;
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 owner=FindHeap(address);
 block=FindChunk(owner,address,0);
 if(M2C_FIELD(block,u32 *,16)!=0) {
  osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);return;
 }
 if(decrement) {count=M2C_FIELD(block,u8 *,22);if(count>0)M2C_FIELD(block,u8 *,22)=count-1;}
 else {count=M2C_FIELD(block,u8 *,22);if(count<255)M2C_FIELD(block,u8 *,22)=count+1;}
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
}
