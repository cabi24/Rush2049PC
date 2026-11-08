/* game.memory (docs/port/MEMORY.md 3, 6): test_forms' scene (looping PCM16 big-endian, ADPCM with a big-endian book,
 * native-order PCM16, the delay effect) as N64-form records in port/ultra's own RDRAM window at 64 MiB (memory.c, in
 * place of the tests' fake RDRAM), built at 1 MiB, at 18 MiB and in the window's last MiB, with the records holding K0
 * addresses or physical ones. Past 16 MiB a physical address no longer fits the N64's 24 bits; the interpreter must
 * take every one as the RAM it is, so each place renders what the first does, sample for sample, and the state and
 * the delay line come out the same. Skipped (77) where the window can't be mapped at PC_RDRAM_BASE. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

/* port/ultra/memory.c, and what it calls of the rest of port/ultra; the ROM the interpreter's table may come from. */
int pc_mem_set_size(uint32_t bytes);
int pc_mem_init(void);
int pc_verbose;
void pc_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}
int pc_fix_absolute_got(void) { return 0; }
const uint8_t *pc_rom_bytes(void) { return NULL; }
size_t pc_rom_size(void) { return 0; }

int test_failures, test_checks;
int test_finish(const char *name)
{
    printf("%s: %d checks, %d failed\n", name, test_checks, test_failures);
    return test_failures ? 1 : 0;
}

#define SUB MX_SUBFRAME
#define NSFD 6
#define LINE (SUB * 6)
enum { PCM_LEN = 6000, ADPCM_LEN = 4096 };

/* Bump allocation from a K0 address in the window; the records' addresses K0 or physical. */
static uint32_t g_next;
static int g_physical;

static void *alloc(size_t size)
{
    uint8_t *p;
    g_next = (g_next + 15) & ~15u;
    p = pc_n64_ptr(g_next);
    g_next += (uint32_t)size;
    if (p)
        memset(p, 0, size);
    return p;
}

static uint32_t addr(const void *p)
{
    uint32_t k0 = p ? pc_n64_addr(p) : 0;
    return k0 && g_physical ? N64_TO_PHYS(k0) : k0;
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v >> 16), put16(p + 2, v);
}

static void voice_to_n64(const PcMusyxVoice *v, uint8_t *p)
{
    for (int k = 0; k < 4; k++) {
        put32(p + MX_V_ENV + 4 * k, (uint32_t)v->env[k]);
        put32(p + MX_V_ENV_STEP + 4 * k, (uint32_t)v->env_step[k]);
    }
    put16(p + MX_V_PITCH_FRAC, v->pitch_frac);
    put16(p + MX_V_PITCH, v->pitch);
    for (int s = 0; s < 2; s++) {
        uint8_t *c = p + MX_V_SRC + s * MX_CAT_SIZE;
        put32(c + MX_CAT_PTR1, addr(v->src[s].ptr1));
        put32(c + MX_CAT_PTR2, addr(v->src[s].ptr2));
        put16(c + MX_CAT_SIZE1, (uint16_t)v->src[s].size1);
        put16(c + MX_CAT_SIZE2, (uint16_t)v->src[s].size2);
    }
    memcpy(p + MX_V_FRAMES, v->frames, 2);
    memcpy(p + MX_V_SKIP, v->skip, 2);
    if (v->frames[0]) {
        put32(p + MX_V_BOOK, addr(v->u.book));
    } else {
        put16(p + MX_V_PCM_COUNT, (uint16_t)v->u.pcm.count[0]);
        put16(p + MX_V_PCM_COUNT + 2, (uint16_t)v->u.pcm.count[1]);
    }
    put32(p + MX_V_OUTPUT, addr(v->output));
    put16(p + MX_V_END, v->end);
    put16(p + MX_V_RESTART, v->restart);
    put16(p + MX_V_4C, v->unused4c);
    put16(p + MX_V_START, v->start);
}

typedef struct {
    int16_t out[NSFD][2 * SUB];
    uint8_t state[MX_STATE_SIZE];
    int16_t line[LINE];
} Result;

/* The scene at K0 address base, rendered; what it gave into r. 0, or -1 when the window has no room there. */
static int scene(uint32_t base, int physical, Result *r)
{
    uint8_t *pcm_be, *adpcm, *n64, *state, *delay;
    int16_t *pcm_native, *line, *out[NSFD], book[128] = {0}, *pcm;
    unsigned seed = 64;
    g_next = base, g_physical = physical;
    if (!pc_n64_ptr(base) || !pc_n64_ptr(base + 0x40000))
        return -1;

    /* the sources, as test_forms makes them */
    pcm_be = alloc(PCM_LEN * 2 + 64);
    pcm_native = alloc(PCM_LEN * 2 + 64);
    adpcm = alloc(256 + ADPCM_LEN / 64 * 40);
    pcm = malloc(ADPCM_LEN * 2);
    for (int k = 0; k < PCM_LEN + 32; k++) {
        int16_t v = (int16_t)lrint(14000 * sin(k * 0.071) + test_rand(&seed) % 2000 - 1000);
        pcm_be[2 * k] = (uint8_t)((uint16_t)v >> 8);
        pcm_be[2 * k + 1] = (uint8_t)v;
        pcm_native[k] = (int16_t)lrint(9000 * sin(k * 0.013));
    }
    for (int k = 0; k < ADPCM_LEN; k++)
        pcm[k] = (int16_t)lrint(11000 * sin(k * 0.05) + 3000 * sin(k * 0.31));
    ref_make_book(book + 6 * 16, 2 * cos(0.05), -1);
    ref_adpcm_encode(pcm, ADPCM_LEN / 32, book, 6, adpcm + 256);
    for (int i = 0; i < 128; i++) {
        adpcm[2 * i] = (uint8_t)((uint16_t)book[i] >> 8);
        adpcm[2 * i + 1] = (uint8_t)book[i];
    }
    free(pcm);

    /* the records, big-endian: the delay, then each SFD and its voices */
    n64 = alloc(NSFD * MX_SFD_SIZE);
    state = alloc(MX_STATE_SIZE);
    line = alloc(LINE * 2);
    delay = alloc(MX_D_SIZE);
    put32(delay + MX_D_BUFFER, addr(line));
    put32(delay + MX_D_LENGTH, LINE);
    put16(delay + MX_D_TAPS, 2);
    put16(delay + MX_D_FEEDBACK, 0x3000);
    put32(delay + MX_D_TAP_DELAY, 500);
    put32(delay + MX_D_TAP_DELAY + 4, 3 * SUB);
    put16(delay + MX_D_TAP_GAIN, 0x2800);
    put16(delay + MX_D_TAP_GAIN + 2, (uint16_t)-0x1800);
    static const uint16_t filter[4] = {0x0800, 0x1000, 0x2000, 0x3000};
    for (int i = 0; i < 4; i++)
        put16(delay + MX_D_FILTER + 2 * i, filter[i]);
    TbVoice tv[3] = {
        {pcm_be, 0, PCM_LEN, 2000, 1111, 1900.5, 0x1C71, {0}, 1},
        {adpcm, 3, ADPCM_LEN - 64, 512, 2048, 300.25, 0x0E39, {0}, 1},
        {(const uint8_t *)pcm_native, 0, PCM_LEN, 0, 0, 5.0, 0x1000, {0}, 1},
    };
    for (int k = 0; k < NSFD; k++) {
        PcMusyxVoice v[3];
        uint8_t *p = n64 + k * MX_SFD_SIZE;
        int n = 0;
        out[k] = alloc(2 * SUB * 2);
        for (int i = 0; i < 3; i++) {
            const int16_t tg[4] = {(int16_t)(0x3000 - 0x400 * k * i), (int16_t)(0x1000 + 0x300 * k),
                                   (int16_t)(0x800 * i), 0x2000};
            if (i == 2 && k >= 4)
                continue;
            tb_voice(&tv[i], &v[n++], tg);
        }
        v[n - 1].output = out[k];
        put16(p + MX_SFD_COUNT, (uint16_t)n);
        put16(p + MX_SFD_DELAY_INDEX, (uint16_t)(k % 6));
        put32(p + MX_SFD_RELEASE_MASK, k == 4 ? 1u << 2 : 0);
        put32(p + MX_SFD_STATE, addr(state));
        put32(p + MX_SFD_DELAY, addr(delay));
        for (int i = 0; i < n; i++)
            voice_to_n64(&v[i], p + MX_SFD_VOICES + i * MX_V_SIZE);
    }
    pc_mem_register_be(pcm_be, PCM_LEN * 2 + 64);
    pc_mem_register_be(adpcm, 256);
    pc_mem_register_be(n64, NSFD * MX_SFD_SIZE);
    pc_mem_register_be(delay, MX_D_SIZE);

    pc_audio_run_task_form(n64, 3, PC_AUDIO_FORM_N64);
    pc_audio_run_task_form(n64 + 3 * MX_SFD_SIZE, NSFD - 3, PC_AUDIO_FORM_N64);

    for (int k = 0; k < NSFD; k++)
        memcpy(r->out[k], out[k], sizeof r->out[k]);
    memcpy(r->state, state, sizeof r->state);
    memcpy(r->line, line, sizeof r->line);
    pc_mem_unregister_be(pcm_be);
    pc_mem_unregister_be(adpcm);
    pc_mem_unregister_be(n64);
    pc_mem_unregister_be(delay);
    return 0;
}

int main(void)
{
    /* 1 MiB in, 18 MiB in and in the 64 MiB window's last MiB: K0 addresses */
    static const uint32_t bases[3] = {PC_RDRAM_BASE + (1u << 20), PC_RDRAM_BASE + (18u << 20),
                                      PC_RDRAM_BASE + (63u << 20)};
    static Result first, r;
    if (pc_mem_set_size(64u << 20) != 0 || pc_mem_init() != 0) {
        printf("window: skipped: port/ultra's RDRAM window can't be mapped here\n");
        return 77;
    }
    for (int b = 0; b < 3; b++)
        for (int phys = 0; phys < 2; phys++) {
            Result *x = b || phys ? &r : &first;
            memset(x, 0, sizeof *x);
            if (scene(bases[b], phys, x) != 0) {
                CHECK(0, "no RAM at 0x%08X in a window of 64 MiB", bases[b]);
                continue;
            }
            if (x == &first) {
                double energy = 0;
                for (int k = 0; k < NSFD; k++)
                    for (int i = 0; i < 2 * SUB; i++)
                        energy += (double)first.out[k][i] * first.out[k][i];
                CHECK(energy > 1e9, "the scene is too quiet to prove anything (%g)", energy);
                continue;
            }
            int bad = 0;
            for (int k = 0; k < NSFD; k++)
                bad += memcmp(first.out[k], r.out[k], sizeof r.out[k]) != 0;
            CHECK(!bad, "at 0x%08X, %s addresses: %d of %d subframes differ from the first place's", bases[b],
                  phys ? "physical" : "K0", bad, NSFD);
            CHECK(!memcmp(first.state, r.state, sizeof r.state), "at 0x%08X, %s addresses: the state differs",
                  bases[b], phys ? "physical" : "K0");
            CHECK(!memcmp(first.line, r.line, sizeof r.line), "at 0x%08X, %s addresses: the delay line differs",
                  bases[b], phys ? "physical" : "K0");
        }
    return test_finish("window");
}
