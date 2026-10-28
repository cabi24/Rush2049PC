/* port/gfx/gfx_tex.h: texture replacement (docs/port/TEXTURES.md): the textures setting (original, upscaled,
 * pack), texture identities and their names, the upscaler's results (in memory and on disk), texture packs and
 * the dump. gfx_rdp.c decodes and uploads; this file decides what to upload instead, and does the slow parts on a
 * worker thread so that a frame never waits for them. */
#ifndef GFX_TEX_H
#define GFX_TEX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { PCGFX_TEX_ORIGINAL = 0, PCGFX_TEX_UPSCALED = 1, PCGFX_TEX_PACK = 2 };

/* A texture's identity: what the tile's texels are, wherever they sit in TMEM and however they were loaded. */
typedef struct {
    uint64_t texels;    /* the raw texel values (CI: the indices) row by row, with the format, size and dimensions */
    uint32_t palette;   /* CI: the TLUT entries the texels use, and the TLUT's type; 0 otherwise */
    uint8_t fmt, siz;   /* G_IM_FMT_*, G_IM_SIZ_*: fmt is G_IM_FMT_CI whenever the TLUT applies */
    uint16_t w, h;      /* the tile's distinct texels: its size, or one period of its mask */
} PcgfxTexId;

/* A pack file's name for it, without ".png": "rgba16_32x32_0123456789abcdef", for CI with the palette's hash
 * after (with_palette) or not: "ci8_32x16_0123456789abcdef_89abcdef". */
void pcgfx_tex_name(const PcgfxTexId *id, int with_palette, char *out, size_t size);
/* A stable 64-bit hash step (the identities and the upscaler's keys: the same on every host). */
uint64_t pcgfx_tex_mix(uint64_t h, uint64_t v);

/* ---------------------------------------------------------------- settings */

/* From the environment (pc_gfx_init): RUSH_GFX_TEXTURES, RUSH_GFX_TEXTURE_PACK, RUSH_GFX_TEXTURE_CACHE,
 * RUSH_GFX_TEXTURE_DUMP, RUSH_GFX_UPSCALE, RUSH_GFX_UPSCALE_2D, RUSH_GFX_TEXTURE_SYNC. */
void pcgfx_tex_configure(void);
int pcgfx_tex_mode(void);
void pcgfx_tex_set_mode(int mode);
/* The variant a draw gets: 0 the original texture, 1 the replaced or upscaled one where there is one. 3D draws
 * (a perspective projection) get 1 in both modes; 2D (texture rectangles, orthographic triangles: the HUD, menus,
 * text) only with a pack, or RUSH_GFX_UPSCALE_2D=1. */
int pcgfx_tex_variant(int is3d);
/* Upscaled mode: how many times bigger (1, 2, 4): the largest power of two up to the internal scale, at most 4,
 * or RUSH_GFX_UPSCALE. Packs: the most a replacement keeps (larger ones are box filtered down to it, else they
 * shimmer): the internal scale. */
int pcgfx_tex_factor(void);
int pcgfx_tex_pack_limit(void);
int pcgfx_tex_dumping(void);
/* Changes whenever textures already uploaded must be made again: the mode, the factor, the pack. */
uint32_t pcgfx_tex_generation(void);

/* ---------------------------------------------------------------- results */

enum { PCGFX_TEX_NONE = 0, PCGFX_TEX_WAIT = 1, PCGFX_TEX_READY = 2 };

/* The upscaled image (w·factor x h·factor) of the w x h image img with key ukey, as the GL texture has it (mask
 * and mirror baked in; rs, rt: it repeats). alpha_indep: alpha is its own channel (not I formats, where it is the
 * intensity). READY with *out; WAIT while it is being made (draw the original meanwhile); NONE for never. The
 * first sight only notes it; from a second frame that sees it, it is queued (the image is kept from the first
 * call, so later calls may pass img NULL). With RUSH_GFX_TEXTURE_SYNC it is made at once. */
int pcgfx_tex_upscaled(uint64_t ukey, int factor, uint32_t frame, const uint8_t *img, int w, int h, int rs, int rt,
                       int alpha_indep, const uint8_t **out);
/* A pack's replacement for id: READY with the image (*w, *h: a whole multiple of id's), WAIT while it loads, NONE. */
int pcgfx_tex_pack(const PcgfxTexId *id, const uint8_t **out, int *w, int *h);
/* The dump (RUSH_GFX_TEXTURE_DUMP): id's texels (id->w x id->h RGBA8) as DIR/<name>.png, once each. */
void pcgfx_tex_dump(const PcgfxTexId *id, const uint8_t *rgba);
/* Each frame: 1 when results arrived since the last call (the cache looks its textures up again). */
int pcgfx_tex_new_frame(uint32_t frame);
void pcgfx_tex_wait_idle(void);   /* tests: until the worker has nothing left */
void pcgfx_tex_counts(unsigned *upscaled, unsigned *from_disk, unsigned *replaced, unsigned *dumped);   /* tests */
void pcgfx_tex_shutdown(void);    /* the worker stopped, every result freed */
void pcgfx_tex_stats_print(void);

/* port/upscale with the edge handling around it (gfx_tex.c): img upscaled factor (2, 4) times into *out
 * (malloc'd). Binary alpha (cut-outs) stays binary, with the upscaler's smooth contour; transparent texels'
 * colours don't bleed into the opaque ones. 0, or -1 when the upscaler can't. */
int pcgfx_tex_upscale_image(const uint8_t *img, int w, int h, int rs, int rt, int alpha_indep, int factor,
                            uint8_t **out);

#ifdef __cplusplus
}
#endif

#endif
