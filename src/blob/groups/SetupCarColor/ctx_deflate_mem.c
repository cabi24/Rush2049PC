#include "game/game.h"

/* The free zip (heap_release/zip.c) gives zlib: three nested one-line functions down to DeallocateMemoryInternal
 * under the heap lock (the N64's code has three levels of inlining there; zcfree is zlib's name). */
typedef unsigned int size_t;
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);

#define local  /* zlib's file-static functions: globals made internal by uld -kp */
#define OF(args) args
#define FAR
#define Z_NULL 0
#define zmemcpy memcpy
#define zmemzero(dest, len) memset(dest, 0, len)
#define Assert(cond, msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c, x)
#define Tracecv(c, x)
#define TRUNCATE_BLOCK
#define check_match(s, start, match, length)

typedef void  FAR *voidpf;

#define Z_NO_FLUSH      0
#define Z_FINISH        4
#define Z_BINARY   0
#define Z_ASCII    1

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
#define MIN_MATCH  3
#define MAX_MATCH  258

#define LENGTH_CODES 29
#define LITERALS  256
#define L_CODES (LITERALS+1+LENGTH_CODES)
#define D_CODES   30
#define BL_CODES  19
#define HEAP_SIZE (2*L_CODES+1)
#define MAX_BITS 15

#define Freq fc.freq
#define Code fc.code
#define Dad  dl.dad
#define Len  dl.len

#define put_byte(s, c) {s->pending_buf[s->pending++] = (c);}
#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
#define MAX_DIST(s)  ((s)->w_size-MIN_LOOKAHEAD)
#define d_code(dist) \
   ((dist) < 256 ? dist_code[dist] : dist_code[256+((dist)>>7)])

#define NIL 0
#define Buf_size (8 * 2*sizeof(char))
#define TOO_FAR 4096

#define NIL 0

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
/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
/* zlib's free: ptr's chunk. */
__inline void zcfree(voidpf ptr)
{
    mem_free(N64A(ptr));
}
#define FREECALL(p) zcfree((voidpf)(p))

#define ZALLOC(size) AllocateMemoryHeap(Z_NULL, (size))
#define TRY_FREE(ptr) { if (ptr) FREECALL(ptr); }

