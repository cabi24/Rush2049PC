/* The records: one scene (looping PCM16 big-endian, ADPCM with a big-endian book, native PCM16, a release mask, the
 * delay effect) rendered from the game's structs (musyx_abi.h's PcMusyx*) in one task and in two; the outputs, the
 * state block and the delay line must be the same. Also the records' layout on this host. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME
#define NSFD 8
#define LINE (SUB * 6)

typedef struct {
    PcMusyxSfd *sfds;
    uint8_t *state;
    int16_t *line;
    PcMusyxDelay *delay;
    int16_t *out[NSFD];
} Scene;

static const uint8_t *pcm_be, *adpcm, *pcm_native;
enum { PCM_LEN = 6000, ADPCM_LEN = 4096 };

/* The scene, from the same voice histories every time. */
static void build(Scene *sc)
{
    TbVoice tv[3] = {
        {pcm_be, 0, PCM_LEN, 2000, 1111, 1900.5, 0x1C71, {0}, 1},
        {adpcm, 3, ADPCM_LEN - 64, 512, 2048, 300.25, 0x0E39, {0}, 1},
        {pcm_native, 0, PCM_LEN, 0, 0, 5.0, 0x1000, {0}, 1},
    };
    sc->sfds = calloc(NSFD, sizeof *sc->sfds);
    sc->state = heap_alloc(MX_STATE_SIZE);
    sc->line = heap_alloc(LINE * 2);
    sc->delay = calloc(1, sizeof *sc->delay);
    sc->delay->buffer = sc->line;
    sc->delay->length = LINE;
    sc->delay->taps = 2;
    sc->delay->feedback = 0x3000;
    sc->delay->tap_delay[0] = 500;
    sc->delay->tap_delay[1] = 3 * SUB;
    sc->delay->tap_gain[0] = 0x2800;
    sc->delay->tap_gain[1] = -0x1800;
    sc->delay->filter[0] = 0x0800;
    sc->delay->filter[1] = 0x1000;
    sc->delay->filter[2] = 0x2000;
    sc->delay->filter[3] = 0x3000;
    for (int k = 0; k < NSFD; k++) {
        PcMusyxSfd *s = &sc->sfds[k];
        int n = 0;
        sc->out[k] = heap_alloc(2 * SUB * 2);
        s->state = sc->state;
        s->delay = sc->delay;
        s->delay_index = (uint16_t)(k % 6);
        for (int i = 0; i < 3; i++) {
            if (i == 2 && k >= 5)
                continue;   /* voice 2 stops after subframe 4 */
            const int16_t tg[4] = {(int16_t)(0x3000 - 0x400 * k * i), (int16_t)(0x1000 + 0x300 * k), (int16_t)(0x800 * i),
                                   0x2000};
            tb_voice(&tv[i], &s->voices[n++], tg);
        }
        s->count = (uint16_t)n;
        s->release_mask = k == 5 ? 1u << 2 : 0;
        s->voices[n - 1].output = sc->out[k];
    }
}

int main(void)
{
    heap_reset();

    /* Sources. */
    {
        uint8_t *p = heap_alloc(PCM_LEN * 2 + 64);
        int16_t *n = heap_alloc(PCM_LEN * 2 + 64);
        uint8_t *a = heap_alloc(256 + ADPCM_LEN / 64 * 40);
        int16_t *pcm = malloc(ADPCM_LEN * 2), book[128] = {0};
        unsigned seed = 64;
        for (int k = 0; k < PCM_LEN + 32; k++) {
            int16_t v = (int16_t)lrint(14000 * sin(k * 0.071) + test_rand(&seed) % 2000 - 1000);
            p[2 * k] = (uint8_t)((uint16_t)v >> 8);
            p[2 * k + 1] = (uint8_t)v;
            n[k] = (int16_t)lrint(9000 * sin(k * 0.013));
        }
        for (int k = 0; k < ADPCM_LEN; k++)
            pcm[k] = (int16_t)lrint(11000 * sin(k * 0.05) + 3000 * sin(k * 0.31));
        ref_make_book(book + 6 * 16, 2 * cos(0.05), -1);
        ref_adpcm_encode(pcm, ADPCM_LEN / 32, book, 6, a + 256);
        for (int i = 0; i < 128; i++) {   /* the book big-endian, as DMA'd from the ROM */
            a[2 * i] = (uint8_t)((uint16_t)book[i] >> 8);
            a[2 * i + 1] = (uint8_t)book[i];
        }
        pc_mem_register_samples(p, PCM_LEN * 2 + 64);
        pc_mem_register_samples(a, 256);
        pcm_be = p;
        adpcm = a;
        pcm_native = (const uint8_t *)n;
        free(pcm);
    }

    static Scene split, whole;   /* static: what they hold lives to the end (LeakSanitizer) */
    build(&split);
    build(&whole);

    pc_audio_run_task(split.sfds, 3);
    pc_audio_run_task(split.sfds + 3, NSFD - 3);
    pc_audio_run_task(whole.sfds, NSFD);

    double energy = 0;
    for (int k = 0; k < NSFD; k++) {
        CHECK(!memcmp(split.out[k], whole.out[k], 2 * SUB * 2), "subframe %d: one task and two differ", k);
        for (int i = 0; i < 2 * SUB; i++)
            energy += (double)split.out[k][i] * split.out[k][i];
    }
    CHECK(energy > 1e9, "scene is too quiet to prove anything (%g)", energy);
    CHECK(!memcmp(split.state, whole.state, MX_STATE_SIZE), "states differ");
    CHECK(!memcmp(split.line, whole.line, LINE * 2), "lines differ");

    /* The records' layout: the game's members in order, pointers widened. On a 32-bit host it is the N64's. */
    printf("records: voice %zu bytes, SFD %zu, delay %zu (pointers %zu)\n", sizeof(PcMusyxVoice),
           sizeof(PcMusyxSfd), sizeof(PcMusyxDelay), sizeof(void *));
    if (sizeof(void *) == 8) {
        CHECK(sizeof(PcMusyxVoice) == 120 && sizeof(PcMusyxSfd) == 3864 && sizeof(PcMusyxDelay) == 80, "LP64 sizes");
        CHECK(offsetof(PcMusyxVoice, src) == 40 && offsetof(PcMusyxVoice, frames) == 88 &&
                  offsetof(PcMusyxVoice, u) == 96 && offsetof(PcMusyxVoice, output) == 104 &&
                  offsetof(PcMusyxVoice, start) == 118,
              "LP64 voice offsets");
    } else if (sizeof(void *) == 4) {
        CHECK(sizeof(PcMusyxVoice) == MX_V_SIZE && sizeof(PcMusyxSfd) == MX_SFD_SIZE && sizeof(PcMusyxDelay) == MX_D_SIZE,
              "ILP32 sizes match the N64");
    }
    CHECK(offsetof(PcMusyxVoice, pitch_frac) == MX_V_PITCH_FRAC && offsetof(PcMusyxVoice, env_step) == MX_V_ENV_STEP,
          "fixed-size head of the voice record");

    return test_finish("forms");
}
