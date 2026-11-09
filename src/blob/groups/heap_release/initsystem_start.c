/* Two start-up helpers of InitSystem (heap_release/initsystem.c): the text lock (MAT3 HAL_text.c HAL_InitText) and
 * the free InitSystem's FreeBuffers uses (MAT3 memsys.c DeallocateMemory); and the frame command buffers'
 * storage. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: the two frame command buffers hold 16-byte commands, so they are larger than the N64's frame_command_buffer_a /
 * frame_command_buffer_b (docs/port/POINTERS.md), and have room for 12 cars in two views: PC_FRAME_MATRICES matrices from +1728
 * and the display list's PC_GLIST_LEN commands from PC_GLIST_OFFSET, where the N64 has 600 and 4000 from +0x9CC0
 * (pc_game_types.h, docs/port/MORECARS.md 8). InitSystem's _InitGfx_Init gives one to each frame record. */
u8 pc_frame_dl[2][PC_FRAME_BYTES] __attribute__((aligned(16)));

extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);

/* Their manifest owners are empty deleted-procedure stubs (src/blob/heap_lock.c, heap_unlock.c): the
 * bodies get written in place (docs/port/PROTOTYPES.md). */
#define heap_lock() osRecvMesg(&heap_lock_queue, 0, 1)
#define heap_unlock() osSendMesg(&heap_lock_queue, 0, 0)

/* Makes the text lock (text_lock_queue, a one-message queue) the first time (MAT3 _InitSemaphore@HAL_text.c). */
static void _InitSemaphore_Init(void)
{
    static s8 sInited = 0; if (!sInited) { sInited = 1; osCreateMesgQueue(&text_lock_queue, text_lock_msg, 1); osSendMesg(&text_lock_queue, (OSMesg)0, 0); }
}

/* The text system's start-up: the lock, and no font chosen (MAT3 HAL_InitText). */
void HAL_InitText_Init(void)
{
    _InitSemaphore_Init();
    gFontIndex = -1;
}

/* Frees the chunk whose data is at ptr, under the heap lock (MAT3 DeallocateMemory). */
void DeallocateMemory_Init(void *ptr)
{
    heap_lock();
    DeallocateMemoryInternal(ptr, 0);
    heap_unlock();
}
