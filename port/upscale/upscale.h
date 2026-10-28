/* port/upscale/upscale.h */
#ifndef PC_UPSCALE_H
#define PC_UPSCALE_H

#include <stdint.h>

/* Upscale an RGBA8 texture 2x into out (2w x 2h RGBA8). wrap_s / wrap_t: 1 if the texture repeats in that
 * direction (neighbours come from the opposite edge), 0 if it is clamped. Returns 0, or -1 if it can't. */
int pc_upscale_rgba2x(const uint8_t *in, int w, int h, int wrap_s, int wrap_t, uint8_t *out);

#endif
