#include "game/game.h"

/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * zip (historical label): the game's one-shot zlib compressor,
 *   deflate_mem(next_in, avail_in, next_out, avail_out, level, windowBits, memLevel) -> total_out.
 * Builds a 28-byte z_stream on the stack, runs zlib 1.0.4 deflateInit2_ (without checks; allocator
 * audio_dma_sync(0, size)), applies configuration_table[level], deflateReset (_tr_init = tr_init),
 * lm_init, one deflate_slow pass (deflate_slow), then deflateEnd: five TRY_FREEs through the
 * heap lock (osRecvMesg(&heap_lock_queue) / DeallocateMemoryInternal(address, 0) / osJamMesg).
 * zlib types as in src/blob/groups/zlib_deflate (zlib (C) 1995-1996 Jean-loup Gailly and Mark Adler).
 *
 * Frontier. Real group: compiled with the locked heap-release sources, so that
 * DeallocateMemoryInternal is internal (address in a1, tag in a2, s0/s1 used unsaved: that is why this
 * function saves s0/s1 without using them).
 * Shaping quirks (compile-affecting):
 *   - the free is THREE nested one-parameter `__inline` functions (zcfree -> mem_free ->
 *     heap_free_locked). Each inline level adds 8 bytes of frame per call site; three levels give
 *     retail's 24-byte spacing of the parked pointers (sp+160/136/112/88) and the 200-byte frame. One
 *     level gives 8, two 16, four 32. The names are mine: the real chain is not identified (the
 *     caller-less stub DeallocateMemory_stub next to DeallocateMemory is probably one level). umerge does not
 *     inline a three-call helper at five sites by itself, hence `__inline`;
 *   - deflateInit2_/deflateReset/lm_init are written out in the body; total_in and total_out are cleared
 *     as two statements, total_in first; `s = strm.state` is re-read for deflateReset;
 *   - the stream state is passed to deflate_slow as strm.state (reloaded), not s.
 */
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

VIEW(char[], heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

__inline void heap_free_locked(unsigned int address)
{
    osRecvMesg(heap_lock_queue, 0, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg(heap_lock_queue, 0, 0);
}
__inline void mem_free(unsigned int address)
{
    heap_free_locked(address);
}
/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
__inline void zcfree(voidpf ptr)
{
    mem_free(N64A(ptr));
}
#define FREECALL(p) zcfree((voidpf)(p))

#define ZALLOC(size) AllocateMemoryHeap(Z_NULL, (size))
#define TRY_FREE(ptr) { if (ptr) FREECALL(ptr); }

