/* End to end, the way the game drives it: per task, four SFDs whose voices the game's builder makes, one
 * per AI buffer; pc_audio_run_task; each buffer to pc_audio_push (osAiSetNextBuffer); the WAV dump read
 * back and measured. Two tones: a looping ADPCM sample (1 kHz at 32 kHz, big-endian book) and a looping
 * PCM16 sample (750 Hz at 24 kHz, big-endian), both played at 22050 Hz. */
#include <stdlib.h>
#include <string.h>

#include "out_wav.h"
#include "test.h"

#define SUB MX_SUBFRAME
#define TASKS 120
#define PER_TASK 4

static PcMusyxSfd sfds[PER_TASK];
static uint8_t state[MX_STATE_SIZE];
static int16_t ai[PER_TASK][2 * SUB];

static void play(const char *wav, TbVoice tv, int16_t vol_l, int16_t vol_r)
{
    const int16_t target[4] = {vol_l, vol_r, 0, 0};
    pc_audio_init("wav", wav);
    pc_audio_set_frequency(22050);
    memset(state, 0, sizeof state);
    for (int t = 0; t < TASKS; t++) {
        for (int k = 0; k < PER_TASK; k++) {
            memset(&sfds[k], 0, sizeof sfds[k]);
            sfds[k].count = 1;
            sfds[k].state = state;
            tb_voice(&tv, &sfds[k].voices[0], target);
            sfds[k].voices[0].output = ai[k];
        }
        pc_audio_run_task(sfds, PER_TASK);
        for (int k = 0; k < PER_TASK; k++)
            pc_audio_push(ai[k], SUB);
    }
    pc_audio_shutdown();
}

static void measure(const char *wav, double want_f, double want_l, double ratio)
{
    int16_t *s = NULL;
    uint32_t rate = 0;
    long n = pc_wav_read(wav, &s, &rate);
    CHECK(n == TASKS * PER_TASK * SUB && rate == 22050, "%s: %ld frames at %u Hz", wav, n, rate);
    if (n <= 0)
        return;
    double f = tone_freq(s, (size_t)n, 2, rate);
    double rl, wl, rr, wr;
    double al = tone_fit2(s + 2 * SUB, (size_t)n - SUB, 2, rate, f, &rl, &wl);
    double ar = tone_fit2(s + 2 * SUB + 1, (size_t)n - SUB, 2, rate, f, &rr, &wr);
    printf("%s: %.4f Hz (expected %.4f), left %.1f (expected about %.0f, residual %.1f rms, %.0f worst), "
           "right %.1f (residual %.1f, %.0f worst)\n",
           wav, f, want_f, al, want_l, rl, wl, ar, rr, wr);
    CHECK(fabs(f - want_f) < 0.02, "frequency %.4f, expected %.4f", f, want_f);
    CHECK(fabs(al - want_l) < want_l * 0.02, "left amplitude %.1f, expected %.1f", al, want_l);
    CHECK(fabs(ar / al - fabs(ratio)) < 0.005, "right/left %.4f, expected %.4f", ar / al, fabs(ratio));
    CHECK(rl < al * 0.02 && rr < ar * 0.02, "residual %.1f / %.1f rms", rl, rr);
    CHECK(wl < al * 0.08 && wr < ar * 0.08, "worst deviation %.0f / %.0f: a click", wl, wr);
    /* left and right in phase (or opposite when the right volume is negative) */
    double dot = 0;
    for (long i = SUB; i < n; i++)
        dot += (double)s[2 * i] * s[2 * i + 1];
    CHECK(ratio > 0 ? dot > 0 : dot < 0, "channel phase");
    free(s);
}

int main(void)
{
    enum { LEN = 6400 };
    static int16_t pcm[LEN];
    static uint8_t adpcm[256 + LEN / 64 * 40], pcm_be[LEN * 2];
    int16_t book[128] = {0};
    double w = 2 * M_PI / 32;   /* 32 samples a cycle */

    /* ADPCM: 1 kHz at 32 kHz, amplitude 20000, loop of 100 cycles from 640. */
    for (int k = 0; k < LEN; k++)
        pcm[k] = (int16_t)lrint(20000 * sin(w * k));
    ref_make_book(book + 5 * 16, 2 * cos(w), -1);
    ref_adpcm_encode(pcm, LEN / 32, book, 5, adpcm + 256);
    for (int i = 0; i < 128; i++) {
        adpcm[2 * i] = (uint8_t)((uint16_t)book[i] >> 8);
        adpcm[2 * i + 1] = (uint8_t)book[i];
    }
    pc_mem_register_be(adpcm, sizeof adpcm);
    {
        uint16_t rate = (uint16_t)lrint(32000.0 / 22050 * 4096);   /* 5944 */
        TbVoice tv = {adpcm, 3, LEN - 64, 640, 3200, 10.0, rate, {0}, 1};
        play("e2e_adpcm.wav", tv, 0x4000, 0x2000);
        /* 22050 * pitch / 32; amplitude 20000 * 0.5 through the 4-tap filter at 1/32 of the input rate */
        measure("e2e_adpcm.wav", 22050.0 * rate / 4096 / 32, 20000 * 0.5 * 0.996, 0.5);
    }

    /* PCM16 big-endian: 750 Hz at 24 kHz, amplitude 16000, right channel inverted at half volume. */
    for (int k = 0; k < LEN; k++) {
        int16_t v = (int16_t)lrint(16000 * sin(w * k + 1.0));
        pcm_be[2 * k] = (uint8_t)((uint16_t)v >> 8);
        pcm_be[2 * k + 1] = (uint8_t)v;
    }
    pc_mem_register_be(pcm_be, sizeof pcm_be);
    {
        uint16_t rate = (uint16_t)lrint(24000.0 / 22050 * 4096);   /* 4458 */
        TbVoice tv = {pcm_be, 0, LEN - 64, 100, 3200, 0.0, rate, {0}, 1};
        play("e2e_pcm.wav", tv, 0x7FFF, -0x4000);
        measure("e2e_pcm.wav", 22050.0 * rate / 4096 / 32, 16000 * (32767 / 32768.0) * 0.996, -0.5);
    }

    return test_finish("e2e");
}
