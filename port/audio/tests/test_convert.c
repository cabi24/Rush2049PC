/* The rate converter from the game's 22047 Hz to a device's (resample.c) and the queue's output formats
 * (audio_out.c), with made-up signals: a flat passband, images of the input's spectrum rejected, DC exact, the
 * channels kept apart, no drift over a long run, the game's 192-frame buffers converted as one stream, and the
 * converter's overshoot of a full-scale signal carried by the float pull, clamped and counted by the 16-bit one. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>

#include "pc_compat.h"
#include "resample.h"
#include "test.h"

#define GAME 22047u   /* what osAiSetFrequency(22050) gives (VI clock / 2208) */

/* Amplitude of a sine at f in channel ch of n int32 stereo frames (least squares on sin and cos). */
static double fit32(const int32_t *x, size_t n, int ch, double rate, double f)
{
    double sc = 0, cc = 0, s2 = 0, c2 = 0;
    for (size_t i = 0; i < n; i++) {
        double w = 2 * M_PI * f * (double)i / rate, s = sin(w), c = cos(w);
        sc += x[2 * i + ch] * s;
        cc += x[2 * i + ch] * c;
        s2 += s * s;
        c2 += c * c;
    }
    return sqrt((sc / s2) * (sc / s2) + (cc / c2) * (cc / c2));
}

static int16_t *tone(size_t n, double f, double amp, int left, int right)
{
    int16_t *s = calloc(n, 4);
    for (size_t i = 0; i < n; i++) {
        double v = amp * sin(2 * M_PI * f * (double)i / GAME);
        if (v > 32767)
            v = 32767;
        if (v < -32768)
            v = -32768;
        s[2 * i] = (int16_t)(left ? lrint(v) : 0);
        s[2 * i + 1] = (int16_t)(right ? lrint(v) : 0);
    }
    return s;
}

/* n game frames through the converter in the game's 192-frame buffers. */
static size_t convert(PcResampler *rs, const int16_t *in, size_t n, int32_t *out)
{
    size_t made = 0;
    for (size_t i = 0; i < n; i += 192)
        made += rs_process32(rs, in + 2 * i, n - i < 192 ? n - i : 192, out + 2 * made);
    return made;
}

int main(void)
{
    static PcResampler rs;
    static int32_t out[48000 * 12 * 2], whole[48000 * 3 * 2];
    const uint32_t rates[] = {48000, 44100};

    for (int r = 0; r < 2; r++) {
        uint32_t dev = rates[r];
        /* 1. Passband: flat to 8 kHz. 2. Images (the input's spectrum mirrored about 22047 Hz) at least 60 dB down. */
        const double fs[] = {100, 1000, 3000, 6000, 8000};
        double worst_gain = 0, worst_image = -200;
        for (int i = 0; i < 5; i++) {
            int16_t *t = tone(GAME, fs[i], 20000, 1, 1);
            rs_init(&rs, GAME, dev);
            size_t n = convert(&rs, t, GAME, out);
            double a = fit32(out + 2 * 1000, n - 2000, 0, dev, fs[i]);
            double img = fit32(out + 2 * 1000, n - 2000, 0, dev, GAME - fs[i]);
            double g = 20 * log10(a / 20000), im = 20 * log10(img / 20000 + 1e-9);
            if (fabs(g) > fabs(worst_gain))
                worst_gain = g;
            if (im > worst_image)
                worst_image = im;
            free(t);
        }
        CHECK(fabs(worst_gain) < 0.05, "%u Hz: passband to 8 kHz within 0.05 dB (worst %+.3f dB)", dev, worst_gain);
        CHECK(worst_image < -60, "%u Hz: images at least 60 dB down (worst %.1f dB)", dev, worst_image);

        /* 3. DC comes out exact. */
        int16_t *dc = tone(GAME, 0, 0, 1, 1);
        for (size_t i = 0; i < GAME; i++)
            dc[2 * i] = dc[2 * i + 1] = 12345;
        rs_init(&rs, GAME, dev);
        size_t n = convert(&rs, dc, GAME, out);
        int dc_ok = 1;
        for (size_t i = 100; i < n - 100; i++)
            dc_ok &= abs(out[2 * i] - 12345) <= 1 && abs(out[2 * i + 1] - 12345) <= 1;
        CHECK(dc_ok, "%u Hz: DC 12345 stays 12345", dev);
        free(dc);

        /* 4. The channels stay apart: a tone on the left leaves the right at 0, and the other way. */
        int16_t *lt = tone(GAME, 997, 25000, 1, 0), *rt = tone(GAME, 997, 25000, 0, 1);
        rs_init(&rs, GAME, dev);
        n = convert(&rs, lt, GAME, out);
        int apart = 1;
        for (size_t i = 0; i < n; i++)
            apart &= out[2 * i + 1] == 0;
        rs_init(&rs, GAME, dev);
        n = convert(&rs, rt, GAME, out);
        for (size_t i = 0; i < n; i++)
            apart &= out[2 * i] == 0;
        CHECK(apart, "%u Hz: left and right stay apart", dev);
        free(lt);
        free(rt);
    }

    /* 5. Ten seconds in the game's buffers: the output's length is the rate ratio's to a frame (no drift; the filter
     * holds back half its taps), and buffer by buffer is the same stream as in one piece (the filter's history carried
     * across calls). */
    {
        int16_t *t = tone(GAME * 10, 440, 16000, 1, 1);
        rs_init(&rs, GAME, 48000);
        size_t n = convert(&rs, t, GAME * 10, out);
        double expect = 10.0 * 48000 - (RS_TAPS / 2) * 48000.0 / GAME;
        CHECK(fabs((double)n - expect) <= 2, "10 s: %zu frames, %.1f expected", n, expect);
        rs_init(&rs, GAME, 48000);
        size_t m = rs_process32(&rs, t, GAME * 2, whole);
        rs_init(&rs, GAME, 48000);
        size_t k = convert(&rs, t, GAME * 2, out);
        CHECK(m == k && !memcmp(whole, out, m * 8), "192-frame buffers: the same stream as one piece");
        free(t);
    }

    /* 6. A tone clipped flat at full scale: converted, it overshoots between the samples. The float pull keeps the
     * overshoot (the device mixes in float); the 16-bit pull clamps it and counts the samples; at half volume nothing
     * clamps, since the volume comes first. */
    {
        int16_t *t = tone(4410, 3000, 60000, 1, 1);
        static float f[48000 * 2];
        static int16_t s[48000 * 2];
        PcAudioStats st;
        setenv("RUSH2049_AUDIO_RATE", "48000", 1);
        pc_audio_init("manual", NULL);
        pc_audio_set_frequency(22050);
        pc_audio_push(t, 4410);
        size_t got = pc_audio_pull_f32(f, 9000);
        float peak = 0;
        for (size_t i = 0; i < 2 * got; i++)
            peak = fabsf(f[i]) > peak ? fabsf(f[i]) : peak;
        CHECK(got > 9000 - 64 && peak > 1.0f && peak < 1.25f, "float pull: overshoot kept, peak %.3f", peak);
        pc_audio_push(t, 4410);
        pc_audio_pull(s, 9000);
        pc_audio_get_stats(&st);
        CHECK(st.clipped > 0, "16-bit pull: %llu samples clamped and counted", (unsigned long long)st.clipped);
        uint64_t before = st.clipped;
        pc_audio_set_volume(0.7);   /* 0.49: well under the overshoot */
        pc_audio_push(t, 4410);
        pc_audio_pull(s, 9000);
        pc_audio_get_stats(&st);
        CHECK(st.clipped == before, "below full volume: nothing clamped (%llu)", (unsigned long long)(st.clipped - before));
        pc_audio_set_volume(1);
        /* in range, the two pulls agree */
        int16_t *q = tone(2205, 500, 9000, 1, 1);
        pc_audio_init("manual", NULL);
        pc_audio_set_frequency(22050);
        pc_audio_push(q, 2205);
        got = pc_audio_pull_f32(f, 4000);
        pc_audio_init("manual", NULL);
        pc_audio_set_frequency(22050);
        pc_audio_push(q, 2205);
        pc_audio_pull(s, 4000);
        int same = 1;
        for (size_t i = 0; i < 2 * got; i++)
            same &= fabsf(f[i] * 32768.0f - s[i]) < 0.01f;
        CHECK(same, "in range: float = 16-bit / 32768");
        pc_audio_shutdown();
        unsetenv("RUSH2049_AUDIO_RATE");
        free(t);
        free(q);
    }
    return test_finish("convert");
}
