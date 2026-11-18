#include "game/game.h"
#include "game/zlib_deflate.h"

/* The free zip (heap_release/zip.c) gives zlib: three nested one-line functions down to DeallocateMemoryInternal
 * under the heap lock (the N64's code has three levels of inlining there; zcfree is zlib's name). */

/* Frees ptr's chunk under the heap lock. */
__inline void heap_free_locked(void *ptr)
{
    lock_take(&heap_lock_queue);
    DeallocateMemoryInternal(ptr, 0);
    lock_give(&heap_lock_queue);
}
/* heap_free_locked. */
__inline void mem_free(void *ptr)
{
    heap_free_locked(ptr);
}
/* zlib's free: ptr's chunk. */
__inline void zcfree(voidpf ptr)
{
    mem_free(ptr);
}
