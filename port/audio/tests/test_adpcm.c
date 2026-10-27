/* ADPCM: frame decode, block layout and skip, codebook selection, and the codec round trip. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

static void put_be16(uint8_t *p, int v)
{
    p[0] = (uint8_t)((uint16_t)v >> 8);
    p[1] = (uint8_t)v;
}

static void random_frame(uint8_t fr[20], unsigned *seed)
{
    for (int i = 0; i < 20; i++)
        fr[i] = (uint8_t)(test_rand(seed) >> 3);
    fr[0] &= 0x3F;   /* modest raw samples */
    fr[2] &= 0x3F;
    fr[4] = (uint8_t)((fr[4] & 0x70) | (8 + (fr[4] & 3)));   /* any book, shifts 8..11 */
}

int main(void)
{
    int16_t book[128], out[64 * 32], ref[32];
    uint8_t fr[4][20], blk[2][40];
    unsigned seed = 2049;

    /* 1. Zero codebook: each sample is its residual, nibbles high first, sign-extended and scaled. */
    memset(book, 0, sizeof book);
    memset(fr[0], 0, 20);
    put_be16(fr[0], 0x1234);
    put_be16(fr[0] + 2, -2);
    fr[0][4] = 0x0C;            /* book 0, shift 12: residual = nibble value */
    fr[0][5] = 0x78;            /* samples 2, 3: +7, -8 */
    fr[0][6] = 0xF1;            /* samples 4, 5: -1, +1 */
    memset(blk, 0, sizeof blk);
    ref_pack_block(fr[0], fr[0], blk[0]);
    musyx_adpcm_decode(out, blk[0], 40, book, 1, 0);
    CHECK(out[0] == 0x1234 && out[1] == -2, "raw samples %d %d", out[0], out[1]);
    CHECK(out[2] == 7 && out[3] == -8 && out[4] == -1 && out[5] == 1 && out[6] == 0, "residuals %d %d %d %d %d",
          out[2], out[3], out[4], out[5], out[6]);
    fr[0][4] = 0x00;            /* shift 0: residual = nibble << 12 */
    ref_pack_block(fr[0], fr[0], blk[0]);
    musyx_adpcm_decode(out, blk[0], 40, book, 1, 0);
    CHECK(out[2] == 28672 && out[3] == -32768 && out[4] == -4096, "shift 0: %d %d %d", out[2], out[3], out[4]);
    fr[0][4] = 0x0E;            /* shift 14: -8 << 12 >> 14 = -2, 7 << 12 >> 14 = 1, -1 << 12 >> 14 = -1 */
    ref_pack_block(fr[0], fr[0], blk[0]);
    musyx_adpcm_decode(out, blk[0], 40, book, 1, 0);
    CHECK(out[2] == 1 && out[3] == -2 && out[4] == -1 && out[5] == 0, "shift 14: %d %d %d %d", out[2], out[3],
          out[4], out[5]);

    /* 2. Book 5 integrates (c0 = 0, c1 = 1.0 everywhere): out[n] = out[1] + sum of residuals 2..n. */
    memset(book, 0, sizeof book);
    for (int i = 0; i < 8; i++)
        book[5 * 16 + 8 + i] = 2048;
    for (int i = 0; i < 20; i++)
        fr[1][i] = (uint8_t)(test_rand(&seed) >> 3);
    put_be16(fr[1], 100);
    put_be16(fr[1] + 2, -300);
    fr[1][4] = 0x5A;            /* book 5, shift 10: residuals -32..28 */
    ref_pack_block(fr[0], fr[1], blk[0]);
    musyx_adpcm_decode(out, blk[0], 40, book, 1, 32);   /* the block's second frame */
    {
        int16_t z[32], zero_book[128] = {0};
        ref_adpcm_frame(fr[1], zero_book, z);   /* residuals alone */
        int sum = -300, ok = 1;
        for (int n = 2; n < 32; n++) {
            sum += z[n];
            ok &= out[n] == sum;
        }
        CHECK(out[0] == 100 && out[1] == -300 && ok, "integrator: out[31] %d expected %d", out[31], sum);
    }

    /* 3. Block layout and skip against the reference, with random books and frames. */
    for (int i = 0; i < 128; i++)
        book[i] = (int16_t)((test_rand(&seed) % 4096) - 2048);
    for (int f = 0; f < 4; f++)
        random_frame(fr[f], &seed);
    ref_pack_block(fr[0], fr[1], blk[0]);
    ref_pack_block(fr[2], fr[3], blk[1]);
    musyx_adpcm_decode(out, blk[0], 80, book, 4, 0);
    for (int f = 0; f < 4; f++) {
        ref_adpcm_frame(fr[f], book, ref);
        CHECK(!memcmp(out + 32 * f, ref, sizeof ref), "frame %d differs from the reference", f);
    }
    for (unsigned skip = 32; skip < 64; skip += 13) {
        musyx_adpcm_decode(out, blk[0], 80, book, 3, skip);
        for (int f = 0; f < 3; f++) {
            ref_adpcm_frame(fr[f + 1], book, ref);
            CHECK(!memcmp(out + 32 * f, ref, sizeof ref), "skip %u: frame %d differs", skip, f);
        }
    }
    /* Past src_len, bytes read as zero: a frame of zeros decodes to silence. */
    musyx_adpcm_decode(out, blk[0], 40, book, 4, 0);
    {
        int silent = 1;
        for (int n = 64; n < 128; n++)
            silent &= out[n] == 0;
        CHECK(silent, "frames past the source should be silent");
    }

    /* 4. The codebook really is an order-2 predictor: encode a 1 kHz sine at 32 kHz with a predictor book
     * (index 3, among random books) and decode it. */
    {
        enum { FRAMES = 1000 };
        int16_t *pcm = malloc(FRAMES * 32 * 2), *dec = malloc(FRAMES * 32 * 2);
        uint8_t *enc = malloc(FRAMES / 2 * 40);
        double w = 2 * M_PI * 1000 / 32000, se = 0, ss = 0;
        for (int n = 0; n < FRAMES * 32; n++)
            pcm[n] = (int16_t)lrint(12000 * sin(w * n) + 3000 * sin(3.1 * w * n));
        ref_make_book(book + 3 * 16, 2 * cos(w), -1);
        ref_adpcm_encode(pcm, FRAMES, book, 3, enc);
        musyx_adpcm_decode(dec, enc, FRAMES / 2 * 40, book, FRAMES, 0);
        for (int n = 0; n < FRAMES * 32; n++) {
            se += (double)(dec[n] - pcm[n]) * (dec[n] - pcm[n]);
            ss += (double)pcm[n] * pcm[n];
        }
        double snr = 10 * log10(ss / se);
        printf("adpcm round trip: SNR %.1f dB\n", snr);
        CHECK(snr > 30, "round-trip SNR %.1f dB", snr);

        /* Zero residuals continue a pure sine from the two raw samples, within the book's rounding. */
        int worst = 0;
        memset(fr[0], 0, 20);
        fr[0][4] = 0x3C;   /* book 3, shift 12, all nibbles 0 */
        for (int n = 0; n < 34; n++)
            pcm[n] = (int16_t)lrint(15000 * sin(w * n + 0.3));
        put_be16(fr[0], pcm[0]);
        put_be16(fr[0] + 2, pcm[1]);
        ref_pack_block(fr[0], fr[0], blk[0]);
        musyx_adpcm_decode(out, blk[0], 40, book, 1, 0);
        for (int n = 0; n < 32; n++) {
            int d = abs(out[n] - pcm[n]);
            worst = d > worst ? d : worst;
        }
        CHECK(worst <= 40, "zero-residual prediction off by %d", worst);   /* undamped: group errors carry */
        free(pcm);
        free(dec);
        free(enc);
    }

    return test_finish("adpcm");
}
