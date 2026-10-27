/* port/audio/out_wav.c: a 16-bit stereo PCM WAV writer. Little-endian on disk whatever the host. The header's
 * sizes are rewritten about once a second of audio, so a dump stays readable if the process dies. */
#include <stdlib.h>
#include <string.h>

#include "out_wav.h"

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v);
    put16(p + 2, v >> 16);
}

static void write_header(PcWav *w)
{
    uint8_t h[44];
    uint64_t bytes64 = w->frames * 4;
    uint32_t bytes = bytes64 > 0xFFFFFFD3u ? 0xFFFFFFD3u : (uint32_t)bytes64;
    memcpy(h, "RIFF", 4);
    put32(h + 4, 36 + bytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);
    put16(h + 20, 1);   /* PCM */
    put16(h + 22, 2);
    put32(h + 24, w->rate);
    put32(h + 28, w->rate * 4);
    put16(h + 32, 4);
    put16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    put32(h + 40, bytes);
    long at = ftell(w->f);
    fseek(w->f, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, w->f);
    if (at > 44)
        fseek(w->f, at, SEEK_SET);
    w->frames_at_patch = w->frames;
}

int pc_wav_open(PcWav *w, const char *path, uint32_t rate)
{
    memset(w, 0, sizeof *w);
    w->f = fopen(path, "wb");
    if (!w->f)
        return -1;
    w->rate = rate;
    write_header(w);
    return 0;
}

void pc_wav_set_rate(PcWav *w, uint32_t rate)
{
    if (w->f && !w->frames) {
        w->rate = rate;
        write_header(w);
    }
}

void pc_wav_write(PcWav *w, const int16_t *frames, size_t n)
{
    uint8_t buf[1024 * 4];
    if (!w->f)
        return;
    while (n) {
        size_t k = n > 1024 ? 1024 : n;
        for (size_t i = 0; i < 2 * k; i++)
            put16(buf + 2 * i, (uint16_t)frames[i]);
        fwrite(buf, 4, k, w->f);
        frames += 2 * k;
        n -= k;
        w->frames += k;
    }
    if (w->frames - w->frames_at_patch >= w->rate) {
        write_header(w);
        fflush(w->f);
    }
}

void pc_wav_close(PcWav *w)
{
    if (!w->f)
        return;
    write_header(w);
    fclose(w->f);
    w->f = NULL;
}

long pc_wav_read(const char *path, int16_t **out, uint32_t *rate)
{
    uint8_t h[12], ck[8];
    FILE *f = fopen(path, "rb");
    long frames = -1;
    int fmt_ok = 0;
    *out = NULL;
    if (!f)
        return -1;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4))
        goto done;
    while (fread(ck, 1, 8, f) == 8) {
        uint32_t size = ck[4] | ck[5] << 8 | ck[6] << 16 | (uint32_t)ck[7] << 24;
        if (!memcmp(ck, "fmt ", 4)) {
            uint8_t fmt[16];
            if (size < 16 || fread(fmt, 1, 16, f) != 16)
                goto done;
            fmt_ok = fmt[0] == 1 && fmt[2] == 2 && fmt[14] == 16;
            *rate = fmt[4] | fmt[5] << 8 | fmt[6] << 16 | (uint32_t)fmt[7] << 24;
            fseek(f, (long)(size - 16), SEEK_CUR);
        } else if (!memcmp(ck, "data", 4)) {
            if (!fmt_ok)
                goto done;
            uint8_t *raw = malloc(size ? size : 1);
            size_t got = fread(raw, 1, size, f);
            frames = (long)(got / 4);
            *out = malloc((size_t)frames * 4 + 4);
            for (long i = 0; i < 2 * frames; i++)
                (*out)[i] = (int16_t)(raw[2 * i] | raw[2 * i + 1] << 8);
            free(raw);
            goto done;
        } else {
            fseek(f, (long)size, SEEK_CUR);
        }
    }
done:
    fclose(f);
    return frames;
}
