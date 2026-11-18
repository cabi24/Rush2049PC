/* include/game/zlib_deflate.h: zlib 1.0.4's deflater as the game has it (MAT3 zip.c): its macros (zconf.h, zlib.h,
 * zutil.h, deflate.h), its types and its two records, the stream and the compressor's state, for its three files:
 * zlib_deflate/group.c (deflate.c and trees.c), heap_release/zip.c (zip, the one-shot compressor CompressGhost calls)
 * and heap_release/zcfree.c. zlib's checks and traces compile to nothing. The tables zlib keeps in data (the static
 * trees, configuration_table, the extra bits) are game/memory.h's, their types (ct_data, static_tree_desc_s, config)
 * game/types.h's. zlib (C) 1995-1996 Jean-loup Gailly and Mark Adler, under the zlib licence. */
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
/* zlib's zcfree, the game's: the heap's free under its lock (zip.c, zcfree.c; deflate.c has its own). */
#define FREECALL(p) zcfree((voidpf)(p))

/* The C library the deflater calls, as its files declare it: sizes are 32-bit counts. */
typedef unsigned int size_t;
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);

/* zconf.h's and deflate.h's types. The deflater counts in 32 bits: uLong and ulg are unsigned int, where zlib's own are
 * unsigned long, and the (long) casts zlib writes are (int) in group.c. */
typedef unsigned char  Byte;
typedef unsigned int   uInt;
typedef unsigned int   uLong;
typedef Byte  FAR Bytef;
typedef char  FAR charf;
typedef int   FAR intf;
typedef void  FAR *voidpf;
typedef unsigned char  uch;
typedef uch FAR uchf;
typedef unsigned short ush;
typedef ush FAR ushf;
typedef unsigned int  ulg;
typedef ush Pos;
typedef Pos FAR Posf;
typedef ush IPos;      /* 1.0.4: unsigned. The game masks IPos values to 16 bits */
typedef struct static_tree_desc_s static_tree_desc;

/* The stream (MAT3 z_stream_s): zlib 1.0.4's without msg and everything after state. The input, the output, and the
 * compressor's state; zip makes one on its stack for each call. */
typedef struct z_stream_s {
    Bytef    *next_in;   /* the next byte to compress */
    uInt     avail_in;   /* bytes left at next_in */
    uLong    total_in;   /* bytes read so far */
    Bytef    *next_out;  /* where the next output byte goes */
    uInt     avail_out;  /* room left at next_out */
    uLong    total_out;  /* bytes written so far: what zip returns */
    struct internal_state FAR *state;
} z_stream;
typedef z_stream FAR *z_streamp;

/* One of the three Huffman trees being built (MAT3 tree_desc_s): its nodes in the state, its largest code with a
 * non-zero frequency, and its static counterpart (static_l_desc, static_d_desc, static_bl_desc). */
typedef struct tree_desc_s {
    ct_data *dyn_tree;
    int     max_code;
    static_tree_desc *stat_desc;
} FAR tree_desc;

/* The compressor's state (MAT3 internal_state, zlib's deflate_state), allocated from the main heap for each zip:
 * zlib 1.0.4's without status, noheader, last_flush, strategy and DEBUG's bits_sent. The fields are zlib's; deflate.h
 * says what each holds. */
typedef struct internal_state {
    z_streamp strm;        /* the stream this is the state of */
    Bytef *pending_buf;    /* the output still to be written to the stream */
    Bytef *pending_out;    /* the next pending byte to write */
    int   pending;         /* bytes in pending_buf */
    Byte  data_type;       /* MAT3's two bytes here; the game's deflater never reads or sets them */
    Byte  method;
    uInt  w_size;          /* the window: 2^w_bits bytes */
    uInt  w_bits;
    uInt  w_mask;
    Bytef *window;
    ulg window_size;
    Posf *prev;
    Posf *head;
    uInt  ins_h;
    uInt  hash_size;
    uInt  hash_bits;
    uInt  hash_mask;
    uInt  hash_shift;
    int block_start;
    uInt match_length;
    IPos prev_match;
    int match_available;
    uInt strstart;
    uInt match_start;
    uInt lookahead;
    uInt prev_length;
    uInt max_chain_length;
    uInt max_lazy_match;
    int level;
    uInt good_match;
    int nice_match;
    struct ct_data_s dyn_ltree[HEAP_SIZE];
    struct ct_data_s dyn_dtree[2*D_CODES+1];
    struct ct_data_s bl_tree[2*BL_CODES+1];
    struct tree_desc_s l_desc;
    struct tree_desc_s d_desc;
    struct tree_desc_s bl_desc;
    ush bl_count[MAX_BITS+1];
    int heap[2*L_CODES+1];
    int heap_len;
    int heap_max;
    uch depth[2*L_CODES+1];
    uchf *l_buf;
    uInt  lit_bufsize;
    uInt last_lit;
    ushf *d_buf;
    ulg opt_len;
    ulg static_len;
    ulg compressed_len;
    uInt matches;
    int last_eob_len;
    ush bi_buf;
    int bi_valid;
} FAR deflate_state;

#endif /* GAME_ZLIB_DEFLATE_H */
