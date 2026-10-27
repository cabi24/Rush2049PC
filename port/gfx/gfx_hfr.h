/* port/gfx/gfx_hfr.h: frame interpolation's statistics and the renderer's test hooks it needs, for tests and tools
 * (gfx_interp.c; GFX.md "High frame rate"). No GBI definitions here, so that tests with their own encoders can
 * include it. */
#ifndef GFX_HFR_H
#define GFX_HFR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t ticks, drawn, early, held;   /* frames shown: in total, drawn between two, the next early, held */
    uint32_t xf, xf_lerp, xf_camera, xf_snap, cuts;   /* the last drawn frame's transforms and its views cut */
    double last_alpha;
} PcGfxHfrStats;
void pcgfx_hfr_stats(PcGfxHfrStats *s);

/* video.frame_rate's text -> pc_gfx_set_frame_rate's value: "original" 0, "display" -1, "120" 120. */
int pc_gfx_parse_frame_rate(const char *text, int *ok);
int pcgfx_gl_read_presented(uint8_t *rgba, int *w, int *h);   /* the presented image (screenshots), top row first */
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);
int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
