/* port/upscale: what the tests and tools/upscale use besides pc_upscale_rgba2x (docs/port/UPSCALE.md). */
#ifndef PC_UPSCALE_INTERNAL_H
#define PC_UPSCALE_INTERNAL_H

#include <stdint.h>

enum {
    PC_UPSCALE_NET,        /* the trained network over Catmull-Rom: what pc_upscale_rgba2x does */
    PC_UPSCALE_CUBIC,      /* Catmull-Rom alone */
    PC_UPSCALE_BILINEAR,   /* bilinear (the GPU's filter on the original texture, sampled at the new texels) */
    PC_UPSCALE_METHODS
};

/* The largest texture accepted, in texels (pc_upscale_rgba2x returns -1 above it): 512 x 512, the second pass of
 * a 4x of the renderer's largest upscaled texture (256 x 256). About 90 MB of scratch at that size. */
#define PC_UPSCALE_MAX_TEXELS (512 * 512)

/* pc_upscale_rgba2x with the colour scaler chosen. Alpha, the fill under transparent texels and the grey and
 * intensity cases are the same for every method. */
int pc_upscale_rgba2x_method(const uint8_t *in, int w, int h, int wrap_s, int wrap_t, uint8_t *out, int method);

/* Threads per texture for the network: 0 (the default) is half the processors, at most 4, each taking at least
 * 8 rows and 1024 texels (so 32 x 32 and smaller run on the caller's thread alone); n asks for n, each taking at
 * least 4 rows and 256 texels. The result doesn't depend on it. Not for use during a call. */
void pc_upscale_set_threads(int n);

/* The network's name and size, for logs and tools. */
extern const char pc_upscale_model_name[];
extern const int pc_upscale_model_params;

#endif
