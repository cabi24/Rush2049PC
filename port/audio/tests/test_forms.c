/* The two record forms: one scene (looping PCM16 big-endian, ADPCM with a big-endian book, native PCM16, a
 * release mask, the delay effect) rendered from native structs, from N64-form bytes in native order and
 * from N64-form bytes big-endian, all in a fake RDRAM. The outputs and states must be identical. Also the
 * native layout on this host. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define SUB MX_SUBFRAME
#define NSFD 8
#define LINE (SUB * 6)

typedef struct {
    PcMusyxSfd *sfds;     /* native form */
    uint8_t *n64;         /* N64 form */
    uint8_t *state;
    int16_t *line;
    PcMusyxDelay *delay;  /* native */
    uint8_t *delay_n64;
    int16_t *out[NSFD];
} Scene;

static const uint8_t *pcm_be, *adpcm, *pcm_native;   /* in the fake RDRAM */
enum { PCM_LEN = 6000, ADPCM_LEN = 4096 };

static void put16(uint8_t *p, uint32_t v, int be)
{
    uint16_t x = (uint16_t)v;
    if (be) {
        p[0] = (uint8_t)(x >> 8);
        p[1] = (uint8_t)x;
    } else {
        memcpy(p, &x, 2);
    }
}

static void put32(uint8_t *p, uint32_t v, int be)
{
    if (be) {
        put16(p, v >> 16, 1);
        put16(p + 2, v, 1);
    } else {
        memcpy(p, &v, 4);
    }
}

static void voice_to_n64(const PcMusyxVoice *v, uint8_t *p, int be)
{
    for (int k = 0; k < 4; k++) {
        put32(p + MX_V_ENV + 4 * k, (uint32_t)v->env[k], be);
        put32(p + MX_V_ENV_STEP + 4 * k, (uint32_t)v->env_step[k], be);
    }
    put16(p + MX_V_PITCH_FRAC, v->pitch_frac, be);
    put16(p + MX_V_PITCH, v->pitch, be);
    for (int s = 0; s < 2; s++) {
        uint8_t *c = p + MX_V_SRC + s * MX_CAT_SIZE;
        put32(c + MX_CAT_PTR1, v->src[s].ptr1 ? rdram_addr(v->src[s].ptr1) : 0, be);
        put32(c + MX_CAT_PTR2, v->src[s].ptr2 ? rdram_addr(v->src[s].ptr2) : 0, be);
        put16(c + MX_CAT_SIZE1, (uint16_t)v->src[s].size1, be);
        put16(c + MX_CAT_SIZE2, (uint16_t)v->src[s].size2, be);
    }
    memcpy(p + MX_V_FRAMES, v->frames, 2);
    memcpy(p + MX_V_SKIP, v->skip, 2);
    if (v->frames[0]) {
        put32(p + MX_V_BOOK, rdram_addr(v->u.book), be);
    } else {
        put16(p + MX_V_PCM_COUNT, (uint16_t)v->u.pcm.count[0], be);
        put16(p + MX_V_PCM_COUNT + 2, (uint16_t)v->u.pcm.count[1], be);
    }
    put32(p + MX_V_OUTPUT, v->output ? rdram_addr(v->output) : 0, be);
    put16(p + MX_V_END, v->end, be);
    put16(p + MX_V_RESTART, v->restart, be);
    put16(p + MX_V_4C, v->unused4c, be);
    put16(p + MX_V_START, v->start, be);
}

static void delay_to_n64(const PcMusyxDelay *d, uint8_t *p, int be)
{
    put32(p + MX_D_BUFFER, rdram_addr(d->buffer), be);
    put32(p + MX_D_LENGTH, d->length, be);
    put16(p + MX_D_TAPS, d->taps, be);
    put16(p + MX_D_FEEDBACK, (uint16_t)d->feedback, be);
    for (int i = 0; i < 8; i++) {
        put32(p + MX_D_TAP_DELAY + 4 * i, d->tap_delay[i], be);
        put16(p + MX_D_TAP_GAIN + 2 * i, (uint16_t)d->tap_gain[i], be);
    }
    for (int i = 0; i < 4; i++)
        put16(p + MX_D_FILTER + 2 * i, (uint16_t)d->filter[i], be);
}

static void sfd_to_n64(const PcMusyxSfd *s, uint8_t *p, const uint8_t *delay_n64, int be)
{
    put16(p + MX_SFD_COUNT, s->count, be);
    put16(p + MX_SFD_DELAY_INDEX, s->delay_index, be);
    put32(p + MX_SFD_RELEASE_MASK, s->release_mask, be);
    put32(p + MX_SFD_STATE, rdram_addr(s->state), be);
    put32(p + MX_SFD_DELAY, s->delay ? rdram_addr(delay_n64) : 0, be);
    for (int i = 0; i < MX_VOICES; i++)
        voice_to_n64(&s->voices[i], p + MX_SFD_VOICES + i * MX_V_SIZE, be);
}

/* Native records whose pointers into the fake RDRAM hold K0 addresses instead (the delay block itself stays
 * a host pointer): what the game's builder stores when its heap is the RDRAM window. */
static void *k0(const void *p)
{
    return p ? (void *)(uintptr_t)rdram_addr(p) : NULL;
}

static void to_k0(Scene *sc)
{
    sc->delay->buffer = k0(sc->delay->buffer);
    for (int k = 0; k < NSFD; k++) {
        PcMusyxSfd *s = &sc->sfds[k];
        s->state = k0(s->state);
        for (int i = 0; i < MX_VOICES; i++) {
            PcMusyxVoice *v = &s->voices[i];
            for (int seg = 0; seg < 2; seg++) {
                v->src[seg].ptr1 = k0(v->src[seg].ptr1);
                v->src[seg].ptr2 = k0(v->src[seg].ptr2);
            }
            if (v->frames[0])
                v->u.book = k0(v->u.book);
            v->output = k0(v->output);
        }
    }
}

/* The scene, from the same voice histories every time. */
static void build(Scene *sc, int be)
{
    TbVoice tv[3] = {
        {pcm_be, 0, PCM_LEN, 2000, 1111, 1900.5, 0x1C71, {0}, 1},
        {adpcm, 3, ADPCM_LEN - 64, 512, 2048, 300.25, 0x0E39, {0}, 1},
        {pcm_native, 0, PCM_LEN, 0, 0, 5.0, 0x1000, {0}, 1},
    };
    sc->sfds = calloc(NSFD, sizeof *sc->sfds);
    sc->n64 = rdram_alloc(NSFD * MX_SFD_SIZE);
    sc->state = rdram_alloc(MX_STATE_SIZE);
    sc->line = rdram_alloc(LINE * 2);
    sc->delay = calloc(1, sizeof *sc->delay);
    sc->delay_n64 = rdram_alloc(MX_D_SIZE);
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
    delay_to_n64(sc->delay, sc->delay_n64, be);
    for (int k = 0; k < NSFD; k++) {
        PcMusyxSfd *s = &sc->sfds[k];
        int n = 0;
        sc->out[k] = rdram_alloc(2 * SUB * 2);
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
        sfd_to_n64(s, sc->n64 + k * MX_SFD_SIZE, sc->delay_n64, be);
    }
}

int main(void)
{
    rdram_reset();

    /* Sources. */
    {
        uint8_t *p = rdram_alloc(PCM_LEN * 2 + 64);
        int16_t *n = rdram_alloc(PCM_LEN * 2 + 64);
        uint8_t *a = rdram_alloc(256 + ADPCM_LEN / 64 * 40);
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
        pc_mem_register_be(p, PCM_LEN * 2 + 64);
        pc_mem_register_be(a, 256);
        pcm_be = p;
        adpcm = a;
        pcm_native = (const uint8_t *)n;
        free(pcm);
    }

    static Scene native, nk0, n64le, n64be;   /* static: what they hold lives to the end (LeakSanitizer) */
    build(&native, 0);
    build(&nk0, 0);
    to_k0(&nk0);
    build(&n64le, 0);
    build(&n64be, 1);
    pc_mem_register_be(n64be.n64, NSFD * MX_SFD_SIZE);
    pc_mem_register_be(n64be.delay_n64, MX_D_SIZE);

    pc_audio_run_task_form(native.sfds, 3, PC_AUDIO_FORM_NATIVE);
    pc_audio_run_task_form(native.sfds + 3, NSFD - 3, PC_AUDIO_FORM_NATIVE);
    pc_audio_run_task_form(nk0.sfds, NSFD, PC_AUDIO_FORM_NATIVE);
    pc_audio_set_task_form(PC_AUDIO_FORM_N64);
    CHECK(pc_audio_task_form() == PC_AUDIO_FORM_N64, "form not set");
    pc_audio_run_task(n64le.n64, 3);
    pc_audio_run_task(n64le.n64 + 3 * MX_SFD_SIZE, NSFD - 3);
    pc_audio_run_task_form(n64be.n64, 3, PC_AUDIO_FORM_N64);
    pc_audio_run_task_form(n64be.n64 + 3 * MX_SFD_SIZE, NSFD - 3, PC_AUDIO_FORM_N64);

    double energy = 0;
    for (int k = 0; k < NSFD; k++) {
        CHECK(!memcmp(native.out[k], nk0.out[k], 2 * SUB * 2), "subframe %d: native form with K0 addresses differs", k);
        CHECK(!memcmp(native.out[k], n64le.out[k], 2 * SUB * 2), "subframe %d: N64 form (native order) differs", k);
        CHECK(!memcmp(native.out[k], n64be.out[k], 2 * SUB * 2), "subframe %d: N64 form (big-endian) differs", k);
        for (int i = 0; i < 2 * SUB; i++)
            energy += (double)native.out[k][i] * native.out[k][i];
    }
    CHECK(energy > 1e9, "scene is too quiet to prove anything (%g)", energy);
    CHECK(!memcmp(native.state, n64le.state, MX_STATE_SIZE) && !memcmp(native.state, n64be.state, MX_STATE_SIZE) &&
              !memcmp(native.state, nk0.state, MX_STATE_SIZE),
          "states differ");
    CHECK(!memcmp(native.line, n64le.line, LINE * 2) && !memcmp(native.line, n64be.line, LINE * 2), "lines differ");

    /* The native layout: the game's members in order, pointers widened. On a 32-bit host it is the N64's. */
    printf("native form: voice %zu bytes, SFD %zu, delay %zu (pointers %zu)\n", sizeof(PcMusyxVoice),
           sizeof(PcMusyxSfd), sizeof(PcMusyxDelay), sizeof(void *));
    if (sizeof(void *) == 8) {
        /* The range test needs host data above 4 GB (a PIE executable). */
        CHECK((uintptr_t)&native >> 32 && (uintptr_t)test_rdram >> 32 && (uintptr_t)native.sfds >> 32,
              "host data below 4 GB: %p %p %p", (void *)&native, (void *)test_rdram, (void *)native.sfds);
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
