#include "game/game.h"
#include "game/zlib_deflate.h"

/* zip (MAT3 zip.c): the game's one-shot compressor, zlib's deflate in one call: a z_stream on the stack, zlib
 * 1.0.4's deflateInit2_ (without its checks), deflateReset and lm_init written out, one deflate_slow pass over all
 * the input, then deflateEnd's frees. CompressGhost compresses the ghosts with it. zlib_deflate/group.c has the rest
 * of zlib (zlib (C) 1995-1996 Jean-loup Gailly and Mark Adler, under the zlib licence). */
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
