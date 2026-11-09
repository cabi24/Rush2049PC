#include "game/game.h"
#include "game/zlib_deflate.h"

/* zip (MAT3 zip.c): the game's one-shot compressor, zlib's deflate in one call: a z_stream on the stack, zlib
 * 1.0.4's deflateInit2_ (without its checks), deflateReset and lm_init written out, one deflate_slow pass over all
 * the input, then deflateEnd's frees. CompressGhost compresses the ghosts with it. zlib_deflate/group.c has the rest
 * of zlib (zlib (C) 1995-1996 Jean-loup Gailly and Mark Adler, under the zlib licence). */
typedef unsigned int size_t;
extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);

typedef unsigned char  Byte;
typedef unsigned int   uInt;
typedef unsigned int  uLong;  /* long is 32-bit on the N64 */
typedef Byte  FAR Bytef;
typedef char  FAR charf;

typedef void  FAR *voidpf;
typedef unsigned char  uch;
typedef uch FAR uchf;
typedef unsigned short ush;
typedef ush FAR ushf;
typedef unsigned int  ulg;

/* the game's z_stream: 1.0.4 minus msg and everything after state */
typedef struct z_stream_s {
    Bytef    *next_in;
    uInt     avail_in;
    uLong    total_in;
    Bytef    *next_out;
    uInt     avail_out;
    uLong    total_out;
    struct internal_state FAR *state;
} z_stream;
typedef z_stream FAR *z_streamp;

typedef struct static_tree_desc_s  static_tree_desc;

typedef struct tree_desc_s {
    ct_data *dyn_tree;
    int     max_code;
    static_tree_desc *stat_desc;
} FAR tree_desc;

typedef ush Pos;
typedef Pos FAR Posf;
typedef ush IPos;      /* 1.0.4: unsigned. The game masks IPos values to 16 bits */

/* the game's deflate_state: 1.0.4 without status, two of noheader /
 * data_type+method / last_flush, strategy, and DEBUG's bits_sent */
typedef struct internal_state {
    z_streamp strm;
    Bytef *pending_buf;
    Bytef *pending_out;
    int   pending;
    int   unk10;          /* not read by these functions */
    uInt  w_size;
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


#define TRY_FREE(ptr) { if (ptr) FREECALL(ptr); }

/* Compresses avail_in bytes at next_in into next_out (avail_out bytes of room) at zlib level level, with a window
 * of 2^windowBits bytes and memLevel's hash; the work buffers come from the main heap and are freed after. Returns
 * the compressed size. */
uLong zip(Bytef *next_in, uInt avail_in, Bytef *next_out, uInt avail_out,
                                 int level, int windowBits, int memLevel)
{
    z_stream strm;
    deflate_state *s;
    ushf *overlay;

    strm.next_in = next_in;
    strm.avail_in = avail_in;
    strm.next_out = next_out;
    strm.avail_out = avail_out;

    s = (deflate_state *) ZALLOC(sizeof(deflate_state));
    strm.total_in = 0;
    strm.total_out = 0;
    strm.state = s;
    s->strm = &strm;

    s->w_bits = windowBits;
    s->w_size = 1 << s->w_bits;
    s->w_mask = s->w_size - 1;

    s->hash_bits = memLevel + 7;
    s->hash_size = 1 << s->hash_bits;
    s->hash_mask = s->hash_size - 1;
    s->hash_shift = ((s->hash_bits + MIN_MATCH - 1) / MIN_MATCH);

    s->window = (Bytef *) ZALLOC(s->w_size * 2 * sizeof(Byte));
    s->prev = (Posf *) ZALLOC(s->w_size * sizeof(Pos));
    s->head = (Posf *) ZALLOC(s->hash_size * sizeof(Pos));

    s->lit_bufsize = 1 << (memLevel + 6);

    overlay = (ushf *) ZALLOC(s->lit_bufsize * (sizeof(ush) + 2));
    s->pending_buf = (uchf *) overlay;
    s->d_buf = overlay + s->lit_bufsize / sizeof(ush);
    s->l_buf = s->pending_buf + (1 + sizeof(ush)) * s->lit_bufsize;

    s->level = level;
    s->good_match = configuration_table[level].good_length;
    s->max_lazy_match = configuration_table[level].max_lazy;
    s->nice_match = configuration_table[level].nice_length;
    s->max_chain_length = configuration_table[level].max_chain;

    /* deflateReset */
    s = strm.state;
    s->pending = 0;
    s->pending_out = s->pending_buf;
    tr_init(s);

    /* lm_init */
    s->window_size = (ulg) 2L * s->w_size;
    s->head[s->hash_size - 1] = NIL;
    zmemzero((charf *) s->head, (unsigned) (s->hash_size - 1) * sizeof(*s->head));
    s->strstart = 0;
    s->block_start = 0L;
    s->lookahead = 0;
    s->match_length = s->prev_length = MIN_MATCH - 1;
    s->match_available = 0;
    s->ins_h = 0;

    deflate_slow(strm.state);

    /* deflateEnd */
    TRY_FREE(s->pending_buf);
    TRY_FREE(s->head);
    TRY_FREE(s->prev);
    TRY_FREE(s->window);
    TRY_FREE(s);
    return strm.total_out;
}
