/* The voice resampler: the 4-tap table (computed, or found in the ROM's microcode data), unity DC gain,
 * pitch ratios, and continuity across subframes and tasks with voices built the way the game builds them
 * (func_80012D18 / func_80011D74), looping PCM16 and ADPCM, against a closed-form reference stream. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME

static PcMusyxSfd sfds[4];
static uint8_t state[MX_STATE_SIZE];

static int32_t clamp16(int32_t v)
{
    return v > 32767 ? 32767 : v < -32768 ? -32768 : v;
}

/* VMULF: rounded half up, saturated. */
static int32_t mulf(int32_t a, int32_t b)
{
    return clamp16((int32_t)(((int64_t)a * b * 2 + 0x8000) >> 16));
}

/* Reference: the whole stream at once, as one pointer and Q16 phase, wrapping at the loop end (frame by frame:
 * the microcode checks once every 8 frames, the same for loops longer than 8 frames' input). Each tap's product
 * rounded and saturated, the pairs and their sum saturated; the volume's product rounded. */
static void ref_stream(const int16_t *in, double pos0, uint16_t pitch, uint32_t loop_start, uint32_t loop_len,
                       int16_t vol, int16_t *out, size_t n)
{
    long p = (long)floor(pos0);
    uint32_t acc = (uint16_t)((pos0 - floor(pos0)) * 65536.0);
    for (size_t i = 0; i < n; i++) {
        const int16_t *tap = musyx_lut[(acc >> 10) & 63];
        p += acc >> 16;
        acc = (acc & 0xFFFF) + ((uint32_t)pitch << 4);
        if (loop_len && p >= (long)(loop_start + loop_len))
            p -= loop_len;
        int32_t x = clamp16(clamp16(mulf(in[p], tap[0]) + mulf(in[p + 1], tap[1])) +
                            clamp16(mulf(in[p + 2], tap[2]) + mulf(in[p + 3], tap[3])));
        out[i] = (int16_t)mulf(x, vol);
    }
}

/* Render subframes of one voice, `per_task` SFDs per task; left channel into out. */
static void render(TbVoice *tv, int16_t vol, int subframes, int per_task, int16_t *out)
{
    static int16_t ai[4][2 * SUB];
    const int16_t target[4] = {vol, 0, 0, 0};
    memset(state, 0, sizeof state);
    for (int k = 0; k < subframes;) {
        int n = subframes - k < per_task ? subframes - k : per_task;
        for (int j = 0; j < n; j++) {
            memset(&sfds[j], 0, sizeof sfds[j]);
            sfds[j].count = 1;
            sfds[j].state = state;
            tb_voice(tv, &sfds[j].voices[0], target);
            sfds[j].voices[0].output = ai[j];
        }
        pc_audio_run_task(sfds, (size_t)n);
        for (int j = 0; j < n; j++, k++)
            for (int i = 0; i < SUB; i++)
                out[k * SUB + i] = ai[j][2 * i];
    }
}

static int compare(const int16_t *a, const int16_t *b, size_t n, size_t *first)
{
    int bad = 0;
    *first = n;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            if (!bad)
                *first = i;
            bad++;
        }
    }
    return bad;
}

int main(void)
{
    enum { N_SUB = 60, LEN = 5056 };
    static int16_t got[N_SUB * SUB], want[N_SUB * SUB], sig[LEN + 64], dec[LEN + 64];
    static uint8_t adpcm[256 + LEN / 64 * 40];
    unsigned seed = 49;
    size_t first;

    /* 1. The table: found in the ROM's microcode data when one is there (a fake ROM here holds a variant of
     * this port's table at data+0x20), else computed. Every row sums to 1.0. */
    {
        int16_t mine[64][4], found[64][4];
        static uint8_t rom[0x2F040], junk[560];
        musyx_lut_compute(mine);
        mine[0][0] += 5;
        mine[0][3] -= 5;
        for (int r = 0; r < 64; r++)
            for (int j = 0; j < 4; j++) {
                rom[0x2EE30 + (r * 4 + j) * 2] = (uint8_t)((uint16_t)mine[r][j] >> 8);
                rom[0x2EE30 + (r * 4 + j) * 2 + 1] = (uint8_t)mine[r][j];
            }
        for (size_t i = 0; i < sizeof junk; i++)
            junk[i] = (uint8_t)test_rand(&seed);
        CHECK(musyx_lut_find(junk, sizeof junk, found) == -1, "random bytes taken for a table");
        test_set_rom(rom, sizeof rom);
        musyx_lut_init();
        test_set_rom(NULL, 0);
        CHECK(!strcmp(musyx_lut_source(), "rom+0x2EE30"), "source %s", musyx_lut_source());
        CHECK(!memcmp(musyx_lut, mine, sizeof mine), "table from the ROM not used");
        musyx_lut_compute(mine);
        int sums = 1, shape = mine[0][1] > 26000 && mine[0][3] == 0 && abs(mine[32][1] - mine[32][2]) <= 2;
        for (int r = 0; r < 64; r++)
            sums &= mine[r][0] + mine[r][1] + mine[r][2] + mine[r][3] == 32768;
        CHECK(sums, "computed rows don't sum to 1.0");
        CHECK(shape, "computed table shape: %d %d %d %d / %d %d", mine[0][0], mine[0][1], mine[0][2], mine[0][3],
              mine[32][1], mine[32][2]);
    }

    /* 2. DC through every phase: 10000, give or take the taps' rounding (each product rounds on its own), and
     * exactly the reference. */
    {
        static int16_t dc[1024];
        for (int k = 0; k < 1024; k++)
            dc[k] = 10000;
        TbVoice tv = {(const uint8_t *)dc, 0, 1000, 0, 0, 10.5, 0x15F3, {0}, 1};
        render(&tv, 0x7FFF, 2, 2, got);
        int bad = 0;
        for (int i = 0; i < 2 * SUB; i++)
            bad += abs(got[i] - 10000) > 2;
        CHECK(!bad, "DC: %d samples not 10000 +- 2 (first %d)", bad, got[0]);
        ref_stream(dc, 10.5, 0x15F3, 0, 0, 0x7FFF, want, 2 * SUB);
        CHECK(!compare(got, want, 2 * SUB, &first), "DC: differs from the reference at %zu", first);
    }

    /* 3. Pitch ratios: a 500 Hz tone comes out at 500 * pitch. */
    {
        static int16_t tone[60000];
        for (int k = 0; k < 60000; k++)
            tone[k] = (int16_t)lrint(12000 * sin(2 * M_PI * 500 * k / 22050.0));
        static const uint16_t pitches[] = {0x0800, 0x1000, 0x1800, 0x2000, 0x1A2B};
        for (size_t t = 0; t < sizeof pitches / sizeof *pitches; t++) {
            TbVoice tv = {(const uint8_t *)tone, 0, 60000, 0, 0, 3.0, pitches[t], {0}, 1};
            render(&tv, 0x7FFF, N_SUB, 3, got);
            double f = tone_freq(got, N_SUB * SUB, 1, 22050), expect = 500.0 * pitches[t] / 4096;
            CHECK(fabs(f - expect) < expect * 2e-4, "pitch %04x: %.3f Hz, expected %.3f", pitches[t], f, expect);
        }
    }

    /* 4. Continuity, PCM16 with a loop: tasks of 1, 2 and 3 SFDs all match the one-stream reference. */
    for (int k = 0; k < LEN + 64; k++)
        sig[k] = (int16_t)(test_rand(&seed) - 16384);
    for (int per = 1; per <= 3; per++) {
        TbVoice tv = {(const uint8_t *)sig, 0, LEN, 1000, 777, 900.25, 0x1A2B, {0}, 1};
        render(&tv, 0x6000, N_SUB, per, got);
        ref_stream(sig, 900.25, 0x1A2B, 1000, 777, 0x6000, want, N_SUB * SUB);
        int bad = compare(got, want, N_SUB * SUB, &first);
        CHECK(!bad, "PCM loop, %d per task: %d differ, first at %zu (%d vs %d)", per, bad, first,
              first < N_SUB * SUB ? got[first] : 0, first < N_SUB * SUB ? want[first] : 0);
    }
    /* A loop shorter than a subframe's window (the whole-loop window with a start offset). */
    {
        TbVoice tv = {(const uint8_t *)sig, 0, LEN, 2000, 300, 2100.5, 0x2000, {0}, 1};
        render(&tv, 0x6000, N_SUB, 2, got);
        ref_stream(sig, 2100.5, 0x2000, 2000, 300, 0x6000, want, N_SUB * SUB);
        int bad = compare(got, want, N_SUB * SUB, &first);
        CHECK(!bad, "short PCM loop: %d differ, first at %zu", bad, first);
    }

    /* 5. Continuity, ADPCM with a loop (book native, index 2), against the reference decode of the same data. */
    {
        int16_t *book = (int16_t *)adpcm;
        double w = 2 * M_PI * 700 / 22050;
        for (int k = 0; k < LEN; k++)
            sig[k] = (int16_t)lrint(9000 * sin(w * k) + 4000 * sin(2.7 * w * k) + (test_rand(&seed) % 600) - 300);
        memset(book, 0, 256);
        ref_make_book(book + 2 * 16, 2 * cos(w), -0.98);
        ref_adpcm_encode(sig, LEN / 32, book, 2, adpcm + 256);
        for (int f = 0; f < LEN / 32; f++) {
            const uint8_t *blk = adpcm + 256 + (f / 2) * 40;
            uint8_t fr[20];
            memcpy(fr, blk + (f & 1) * 4, 4);
            memcpy(fr + 4, blk + 8 + (f & 1) * 16, 16);
            ref_adpcm_frame(fr, book, dec + f * 32);
        }
        for (int per = 1; per <= 3; per += 2) {
            TbVoice tv = {adpcm, 3, LEN - 64, 1234, 1500, 1100.75, 0x1C00, {0}, 1};
            render(&tv, 0x7000, N_SUB, per, got);
            ref_stream(dec, 1100.75, 0x1C00, 1234, 1500, 0x7000, want, N_SUB * SUB);
            int bad = compare(got, want, N_SUB * SUB, &first);
            CHECK(!bad, "ADPCM loop, %d per task: %d differ, first at %zu", per, bad, first);
        }
    }

    return test_finish("resample");
}
