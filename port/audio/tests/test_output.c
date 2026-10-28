/* Output: the queue (counts in game frames, pulls, underruns, overflow), the rate converter (ratio,
 * frequency, amplitude, alias rejection, chunking), the WAV writer, and the null backend's clock. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "out_wav.h"
#include "pc_compat.h"
#include "resample.h"
#include "test.h"

static int16_t *tone(size_t n, double freq, double rate, double amp)
{
    int16_t *s = malloc(n * 4);
    for (size_t i = 0; i < n; i++) {
        s[2 * i] = (int16_t)lrint(amp * sin(2 * M_PI * freq * i / rate));
        s[2 * i + 1] = (int16_t)lrint(-amp * sin(2 * M_PI * freq * i / rate));
    }
    return s;
}

static size_t convert(PcResampler *rs, const int16_t *in, size_t n, int16_t *out, unsigned *seed)
{
    size_t made = 0;
    while (n) {
        size_t k = seed ? 1 + test_rand(seed) % 700 : n;
        if (k > n)
            k = n;
        made += rs_process(rs, in, k, out + 2 * made);
        in += 2 * k;
        n -= k;
    }
    return made;
}

static double fake_seconds;
static int hook_calls;

static double fake_clock(void) { return fake_seconds; }
static void count_hook(void) { hook_calls++; }

int main(void)
{
    static int16_t buf[48000 * 2 * 2];
    PcAudioStats st;
    unsigned seed = 5;

    /* 1. The manual queue. */
    CHECK(pc_audio_init("manual", NULL) == 0, "manual init");
    pc_audio_set_frequency(22050);
    CHECK(pc_audio_device_rate() == 22050, "device rate %u", pc_audio_device_rate());
    {
        int16_t *t = tone(1000, 440, 22050, 8000);
        pc_audio_push(t, 1000);
        CHECK(pc_audio_queued_frames() == 1000, "queued %zu", pc_audio_queued_frames());
        CHECK(pc_audio_pull(buf, 600) == 600 && !memcmp(buf, t, 600 * 4), "first pull");
        CHECK(pc_audio_queued_frames() == 400, "queued after pull %zu", pc_audio_queued_frames());
        /* An underrun: the last frame fades to silence over 32 frames, then zeros. */
        CHECK(pc_audio_pull(buf, 600) == 400 && !memcmp(buf, t + 1200, 400 * 4), "short pull");
        int fade_ok = 1;
        for (int i = 0; i < 32; i++)
            fade_ok &= buf[2 * (400 + i)] == t[2 * 999] * (31 - i) / 32 &&
                       buf[2 * (400 + i) + 1] == t[2 * 999 + 1] * (31 - i) / 32;
        CHECK(fade_ok && buf[2 * 432] == 0 && buf[2 * 599 + 1] == 0, "fade out: %d %d from %d %d", buf[800], buf[801],
              t[1998], t[1999]);
        pc_audio_get_stats(&st);
        CHECK(st.underrun_frames == 200 && st.underruns == 1 && st.pushed_frames == 1000 && st.idle_frames == 0,
              "stats %llu %llu %llu %llu", (unsigned long long)st.underrun_frames, (unsigned long long)st.underruns,
              (unsigned long long)st.pushed_frames, (unsigned long long)st.idle_frames);
        /* Another empty pull is the same underrun, all zeros. */
        pc_audio_pull(buf, 100);
        pc_audio_get_stats(&st);
        CHECK(st.underruns == 1 && st.underrun_frames == 300 && buf[0] == 0 && buf[199] == 0, "still one underrun");
        free(t);
        /* Overflow keeps the newest second; after the underrun the sound fades in over 32 frames. */
        t = tone(30000, 300, 22050, 8000);
        pc_audio_push(t, 30000);
        pc_audio_get_stats(&st);
        CHECK(pc_audio_queued_frames() == 22050 && st.dropped_frames == 7950, "overflow: queued %zu dropped %llu",
              pc_audio_queued_frames(), (unsigned long long)st.dropped_frames);
        pc_audio_pull(buf, 64);
        int in_ok = 1;
        for (int i = 0; i < 32; i++)
            in_ok &= buf[2 * i] == t[2 * (7950 + i)] * (i + 1) / 32;
        CHECK(in_ok && !memcmp(buf + 64, t + 2 * (7950 + 32), 32 * 4), "the oldest frames go first, faded in");
        free(t);
    }
    pc_audio_shutdown();

    /* 1b. The AI's target: two pulls, a retrace and a buffer; a buffer more per underrun, at most 200 ms; a floor
     * from audio.latency. Silence while the game pushes nothing is idle, not an underrun; so is a pause. The pull
     * hook; buffers pushed again before a render. */
    {
        int16_t *t = tone(2000, 440, 22050, 8000);
        pc_audio_init("manual", NULL);
        pc_audio_set_frequency(22050);
        CHECK(pc_audio_target_frames() == 384, "target before a pull %zu", pc_audio_target_frames());
        pc_audio_set_clock(fake_clock);
        fake_seconds = 10;
        pc_audio_pull(buf, 500);   /* before the first push: idle */
        pc_audio_get_stats(&st);
        CHECK(st.underruns == 0 && st.idle_frames == 500, "idle before the game plays: %llu %llu",
              (unsigned long long)st.underruns, (unsigned long long)st.idle_frames);
        size_t t0 = 2 * 500 + 22050 / 60 + 192;
        CHECK(pc_audio_target_frames() == t0 && st.period_frames == 500, "target from pulls of 500: %zu (expected %zu)",
              pc_audio_target_frames(), t0);
        pc_audio_push(t, 300);
        pc_audio_pull(buf, 400);   /* runs out while playing */
        pc_audio_get_stats(&st);
        CHECK(st.underruns == 1 && pc_audio_target_frames() == t0 + 192, "an underrun grows the target: %zu",
              pc_audio_target_frames());
        for (int i = 0; i < 100; i++) {
            pc_audio_push(t, 10);
            pc_audio_pull(buf, 20);
        }
        CHECK(pc_audio_target_frames() == 22050 / 5, "at most 200 ms: %zu", pc_audio_target_frames());
        pc_audio_get_stats(&st);
        uint64_t u = st.underruns;
        fake_seconds = 11;         /* nothing pushed for a second: idle */
        pc_audio_pull(buf, 400);
        pc_audio_get_stats(&st);
        CHECK(st.underruns == u, "a quiet game isn't an underrun");
        pc_audio_push(t, 100);
        pc_audio_set_paused(1);
        CHECK(pc_audio_target_frames() == 0, "paused: the AI asks for nothing");
        pc_audio_pull(buf, 400);
        pc_audio_get_stats(&st);
        CHECK(st.underruns == u, "paused: not an underrun");
        pc_audio_set_paused(0);
        pc_audio_set_clock(NULL);
        pc_audio_shutdown();

        pc_audio_init("manual", NULL);
        pc_audio_set_frequency(22050);
        pc_audio_set_latency(100);
        CHECK(pc_audio_target_frames() == 2205, "a 100 ms floor: %zu", pc_audio_target_frames());
        pc_audio_set_latency(0);
        CHECK(pc_audio_target_frames() == 384, "no floor: %zu", pc_audio_target_frames());
        pc_audio_set_pull_hook(count_hook);
        hook_calls = 0;
        pc_audio_pull(buf, 10);
        pc_audio_pull(buf, 10);
        CHECK(hook_calls == 2, "the pull hook runs after each pull: %d", hook_calls);
        pc_audio_set_pull_hook(NULL);

        /* A buffer pushed again before a task rendered it plays its last turn again. */
        static int16_t a[2 * 192], b[2 * 192];
        pc_audio_note_rendered(a);
        pc_audio_note_rendered(b);
        pc_audio_push(a, 192);
        pc_audio_push(b, 192);
        pc_audio_note_rendered(a);
        pc_audio_push(a, 192);
        pc_audio_get_stats(&st);
        CHECK(st.replayed == 0, "rendered buffers: %llu replayed", (unsigned long long)st.replayed);
        pc_audio_push(b, 192);
        pc_audio_get_stats(&st);
        CHECK(st.replayed == 1, "b again without a render: %llu replayed", (unsigned long long)st.replayed);
        pc_audio_shutdown();
        free(t);
    }

    /* 2. 22050 -> 48000: ratio, frequency, amplitude; chunked equals whole. */
    {
        static PcResampler rs;
        static int16_t a[60000 * 2], b[60000 * 2];
        int16_t *t = tone(22050, 1000, 22050, 10000);
        rs_init(&rs, 22050, 48000);
        size_t na = convert(&rs, t, 22050, a, NULL);
        rs_init(&rs, 22050, 48000);
        size_t nb = convert(&rs, t, 22050, b, &seed);
        CHECK(na == nb && !memcmp(a, b, na * 4), "chunked conversion differs (%zu vs %zu)", na, nb);
        CHECK(labs((long)na - 48000) <= 64, "48000 expected, %zu made", na);
        double resid, f = tone_freq(a + 200, na - 400, 2, 48000), amp = tone_fit(a + 200, na - 400, 2, 48000, f, &resid);
        CHECK(fabs(f - 1000) < 0.05, "frequency %.4f", f);
        CHECK(fabs(amp - 10000) < 50 && resid < 20, "amplitude %.1f residual %.2f", amp, resid);
        double ampr = tone_fit(a + 201, na - 400, 2, 48000, f, NULL);
        CHECK(fabs(ampr - 10000) < 50, "right amplitude %.1f", ampr);
        free(t);

        /* 3. 48000 -> 22050: a 15 kHz tone (above the new Nyquist) is rejected, 1 kHz passes. */
        t = tone(48000, 15000, 48000, 10000);
        rs_init(&rs, 48000, 22050);
        na = convert(&rs, t, 48000, a, &seed);
        double alias = tone_fit(a + 400, na - 800, 2, 22050, 22050 - 15000, NULL);
        CHECK(alias < 10000 * 0.01, "15 kHz alias at 7.05 kHz: %.1f", alias);
        free(t);
        t = tone(48000, 1000, 48000, 10000);
        rs_init(&rs, 48000, 22050);
        na = convert(&rs, t, 48000, a, &seed);
        CHECK(labs((long)na - 22050) <= 40, "22050 expected, %zu made", na);
        amp = tone_fit(a + 200, na - 400, 2, 22050, 1000, &resid);
        CHECK(fabs(amp - 10000) < 60 && resid < 30, "1 kHz down: amplitude %.1f residual %.2f", amp, resid);
        free(t);
    }

    /* 4. The queue converts to a forced device rate, and counts in game frames. */
    setenv("RUSH2049_AUDIO_RATE", "48000", 1);
    pc_audio_init("manual", NULL);
    pc_audio_set_frequency(22050);
    CHECK(pc_audio_device_rate() == 48000, "forced rate %u", pc_audio_device_rate());
    {
        int16_t *t = tone(22050, 500, 22050, 9000);
        for (int i = 0; i < 22050; i += 192)
            pc_audio_push(t + 2 * i, 22050 - i < 192 ? 22050 - i : 192);
        size_t q = pc_audio_queued_frames();
        CHECK(labs((long)q - 22050) <= 40, "queued %zu game frames", q);
        size_t got = pc_audio_pull(buf, 48000);
        CHECK(labs((long)got - 48000) <= 64, "pulled %zu device frames", got);
        double f = tone_freq(buf + 200, got - 400, 2, 48000);
        CHECK(fabs(f - 500) < 0.05, "500 Hz through the queue: %.4f", f);
        free(t);
    }
    pc_audio_shutdown();
    unsetenv("RUSH2049_AUDIO_RATE");

    /* 5. The WAV dump holds exactly what was pushed, at the game's rate. */
    {
        int16_t *t = tone(5000, 700, 32000, 12000), *back = NULL;
        uint32_t rate = 0;
        CHECK(pc_audio_init("wav", "test_output.wav") == 0, "wav init");
        pc_audio_set_frequency(32000);
        CHECK(!strcmp(pc_audio_backend_name(), "wav"), "backend %s", pc_audio_backend_name());
        pc_audio_push(t, 3000);
        pc_audio_push(t + 6000, 2000);
        pc_audio_shutdown();
        long n = pc_wav_read("test_output.wav", &back, &rate);
        CHECK(n == 5000 && rate == 32000 && back && !memcmp(back, t, 5000 * 4), "wav read back: %ld frames at %u", n,
              rate);
        free(t);
        free(back);
    }

    /* 6. The null backend drains in real time. */
    {
        int16_t *t = tone(22050, 440, 22050, 1000);
        struct timespec ts = {0, 200 * 1000 * 1000};
        pc_audio_init("null", NULL);
        pc_audio_set_frequency(22050);
        pc_audio_push(t, 22050);
        size_t q0 = pc_audio_queued_frames();
        nanosleep(&ts, NULL);
        size_t q1 = pc_audio_queued_frames();
        long drained = (long)q0 - (long)q1;
        CHECK(drained > 3000 && drained < 8000, "drained %ld in 200 ms (expected about 4410)", drained);
        pc_audio_shutdown();
        free(t);
    }

    return test_finish("output");
}
