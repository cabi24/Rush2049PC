/* include/game/zlib_deflate.h: zlib 1.0.4's macros (zconf.h, zlib.h, zutil.h, deflate.h) as the game's deflater has
 * them, for its three files: zlib_deflate/group.c (deflate.c and trees.c), heap_release/zip.c (zip, the one-shot
 * compressor CompressGhost calls) and SetupCarColor/ctx_deflate_mem.c. zlib's checks and traces compile to nothing.
 * zlib (C) 1995-1996 Jean-loup Gailly and Mark Adler, under the zlib licence. */
#ifndef GAME_ZLIB_DEFLATE_H
#define GAME_ZLIB_DEFLATE_H

/* zconf.h and zutil.h */
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

/* zlib.h: the flush values and data types the game uses */
#define Z_NO_FLUSH      0
#define Z_FINISH        4
#define Z_BINARY   0
#define Z_ASCII    1

/* deflate.h: the block types, the match lengths, the codes, the trees' fields */
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

/* deflate.c and trees.c */
#define NIL 0
#define Buf_size (8 * 2*sizeof(char))
#define TOO_FAR 4096
/* The work buffers come from the main heap. */
#define ZALLOC(size) AllocateMemoryHeap(Z_NULL, (size))
/* zlib's zcfree, the game's: the heap's free under its lock (zip.c, ctx_deflate_mem.c; deflate.c has its own). */
#define FREECALL(p) zcfree((voidpf)(p))

#endif /* GAME_ZLIB_DEFLATE_H */
