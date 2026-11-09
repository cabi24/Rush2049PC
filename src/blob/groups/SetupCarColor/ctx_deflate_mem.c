#include "game/game.h"
#include "game/zlib_deflate.h"

/* The free zip (heap_release/zip.c) gives zlib: three nested one-line functions down to DeallocateMemoryInternal
 * under the heap lock (the N64's code has three levels of inlining there; zcfree is zlib's name). */
typedef unsigned int size_t;
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);

typedef void  FAR *voidpf;

extern int osRecvMesg(void *mq, void *msg, int flag);
extern int osSendMesg(void *mq, void *msg, int flag);

/* Frees the chunk at window address address under the heap lock. */
__inline void heap_free_locked(unsigned int address)
{
    osRecvMesg(&heap_lock_queue, 0, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg(&heap_lock_queue, 0, 0);
}
/* heap_free_locked. */
__inline void mem_free(unsigned int address)
{
    heap_free_locked(address);
}
/* zlib's free: ptr's chunk. */
__inline void zcfree(voidpf ptr)
{
    mem_free(PC_N64A(ptr));
}

#define TRY_FREE(ptr) { if (ptr) FREECALL(ptr); }
