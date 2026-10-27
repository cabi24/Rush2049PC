/* port/audio/musyx_lut.c: the voice resampler's 4-tap filter table.
 *
 * The microcode keeps a 64-phase table in its data (the task's ucode_data, 0x8002E210, 560 bytes at ROM
 * 0x2EE10). It is Nintendo's, so this port doesn't carry it: at the first task it looks for a table of the
 * expected shape in the player's ROM and uses it if found, and otherwise uses its own design, a Hann-windowed
 * sinc low-pass at 0.8 of the input Nyquist frequency (which, like the microcode's, smooths even at phase 0). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "musyx.h"
#include "pc_port.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define UCODE_DATA_ROM 0x2EE10u
#define UCODE_DATA_SIZE 0x230u

int16_t musyx_lut[64][4];
static int lut_ready;
static char lut_source[32] = "computed";

void musyx_lut_compute(int16_t lut[64][4])
{
    const double fc = 0.8;
    for (int r = 0; r < 64; r++) {
        double phase = r / 64.0, h[4], sum = 0;
        for (int j = 0; j < 4; j++) {
            double d = j - 1 - phase;   /* tap distance from the interpolation point */
            double x = M_PI * fc * d;
            double sinc = fabs(x) < 1e-12 ? 1.0 : sin(x) / x;
            double w = fabs(d) < 2 ? 0.5 * (1 + cos(M_PI * d / 2)) : 0;
            h[j] = sinc * w;
            sum += h[j];
        }
        int q[4], total = 0, big = 0;
        for (int j = 0; j < 4; j++) {
            q[j] = (int)lround(h[j] / sum * 32768.0);
            total += q[j];
            if (fabs(h[j]) > fabs(h[big]))
                big = j;
        }
        q[big] += 32768 - total;   /* rows sum to exactly 1.0 */
        for (int j = 0; j < 4; j++)
            lut[r][j] = (int16_t)(q[j] > 32767 ? 32767 : q[j]);
    }
}

/* A resample table here has rows that each sum to about 1.0, a dominant tap 1 at phase 0 that falls with the
 * phase, and a tap 2 that rises to dominate at the last phase. */
static int plausible(const uint8_t *p, int16_t t[64][4])
{
    for (int r = 0; r < 64; r++) {
        int sum = 0;
        for (int j = 0; j < 4; j++) {
            t[r][j] = (int16_t)(p[(r * 4 + j) * 2] << 8 | p[(r * 4 + j) * 2 + 1]);
            sum += t[r][j];
        }
        if (sum < 32768 - 1024 || sum > 32768 + 1024)
            return 0;
        if (r && (t[r][1] > t[r - 1][1] + 64 || t[r][2] < t[r - 1][2] - 64))
            return 0;
    }
    return t[0][1] > 16384 && t[63][2] > 16384 && t[0][1] > t[0][2] && t[63][2] > t[63][1];
}

long musyx_lut_find(const uint8_t *data, size_t size, int16_t lut[64][4])
{
    int16_t t[64][4];
    for (size_t o = 0; o + 512 <= size; o += 2) {
        if (plausible(data + o, t)) {
            memcpy(lut, t, sizeof t);
            return (long)o;
        }
    }
    return -1;
}

void musyx_lut_init(void)
{
    if (lut_ready)
        return;
    musyx_lut_compute(musyx_lut);
    const uint8_t *rom = pc_rom_bytes();
    if (rom && pc_rom_size() >= UCODE_DATA_ROM + UCODE_DATA_SIZE) {
        long o = musyx_lut_find(rom + UCODE_DATA_ROM, UCODE_DATA_SIZE, musyx_lut);
        if (o >= 0)
            snprintf(lut_source, sizeof lut_source, "rom+0x%lX", (unsigned long)UCODE_DATA_ROM + (unsigned long)o);
    }
    fprintf(stderr, "audio: resample table: %s\n", lut_source);
    lut_ready = 1;
}

const char *musyx_lut_source(void)
{
    return lut_source;
}
