/* port/assets/tests/test_text.c: the text bank converter (port/assets/text.c) on synthetic big-endian banks, then
 * the game's own _AdjustTextWavePointers and three readers that view gText differently: by field
 * (func_803AE51C's table overlay through D_8017A4E4) and by raw N64 offsets (voice_stop). A second bank in the same
 * block stands in for the language switch.
 * game: src/blob/groups/slot_sound/func_800A4E58.c src/ovl_a/func_803AE51C.c
 * game: src/blob/groups/pausescr_state/voice.c
 */
#include <stdarg.h>
#include <string.h>

#include "assets.h"
#include "support.h"

char **pc_text_strings(int section, const uint32_t *offsets, int count, void *base);

/* gText as the game's PC views see it (host layout of ModelTables), with countdown_state and countdown_object
 * (D_8017A4E4, the single table) at the addresses port/data gives them. */
struct TextHeader {
    uint16_t count0;
    char **single;
    uint16_t count1, count2;
    uint16_t *tab;
    char **taba;
    uint16_t count3, count4, count5;
    uint16_t *tab2, *tab2a;
    char **tab2b;
} D_8017A4E0;
__asm__(".globl countdown_state\n.set countdown_state, D_8017A4E0\n"
        ".globl D_8017A4E4\n.set D_8017A4E4, D_8017A4E0 + 8\n");

/* Stand-ins for what the game files reference. */
static uint8_t *g_bank;
int32_t D_80140AF0;
void *slot_value_get(int32_t index) { return g_bank; }

int8_t D_801613E8;
uint8_t D_803B8D64[4];
static void *g_drawn;
static int32_t g_value;
void func_800B65B4(float v) {}
void func_800B669C(int32_t a, int32_t b) {}
void func_800B42F0(int32_t id) {}
void func_800B74A0(int32_t id) {}
void func_800BE078(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color, int32_t flags, void *text) { g_drawn = text; }
void func_800B71D4(int32_t x, int32_t y, int32_t value) { g_value = value; }

uint8_t D_801461D0[24], D_80117280[4], D_8014A160[76 * 2];
char D_801210B0[] = "%d %s", D_801210C4[] = "%s %s";
int32_t D_801170FC, D_8015698C;
int8_t D_80117350;
void *D_8011734C;
static const char *g_formatted, *g_measured;
int32_t osRecvMesg(void *q, void *m, int32_t f) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t f) { return 0; }
int32_t slot_state_setup(int32_t font) { return 0; }
int16_t object_bytes_sum_global(void) { return 8; }
void *ambient_sound_set(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g, int32_t h) { return &g_bank; }
void crowd_cheer_play(void *p, int32_t a, int32_t b, int32_t c, int32_t d) {}
int32_t object_manager_update(uint8_t *s, int16_t max) { g_measured = (const char *)s; return 10; }
void fcvt_wrapper(char *out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (fmt == D_801210B0)
        (void)va_arg(ap, int);
    else
        (void)va_arg(ap, char *);
    g_formatted = va_arg(ap, const char *);
    va_end(ap);
    strcpy(out, "x");
}

void func_800A4E58(void);
int32_t func_803AE51C(int32_t arg0);
void voice_stop(void);

/* Section sizes of the synthetic banks. */
enum { N0 = 240, N1 = 60, N2 = 12, N3 = 3, N4 = 1, N5 = 2 };

/* Writes a big-endian bank: string offsets point to "<tag><section letter><index>", tab[i] = i * 3 + seed,
 * tab[56] = 3. Returns the file size. */
static size_t make_bank(uint8_t *f, char tag, uint16_t seed)
{
    static const int counts[6] = {N0, N1, N2, N3, N4, N5};
    static const char letter[6] = {'S', 0, 'A', 0, 0, 'B'};
    size_t pos = 0, str, tables[6];
    int s, i;

    for (s = 0; s < 6; s++) {   /* table positions first, strings after the last table */
        tables[s] = pos;
        pos += 4 + (letter[s] ? counts[s] * 4 : ((counts[s] + 1) & ~1) * 2);
    }
    str = pos;
    for (s = 0; s < 6; s++) {
        uint8_t *t = f + tables[s];
        test_put32(t, (uint32_t)counts[s]);
        for (i = 0; i < counts[s]; i++) {
            if (letter[s]) {
                test_put32(t + 4 + 4 * i, (uint32_t)str);
                str += (size_t)sprintf((char *)f + str, "%c%c%d", tag, letter[s], i) + 1;
            } else
                test_put16(t + 4 + 2 * i, (uint16_t)(s == 1 && i == 56 ? 3 : i * 3 + seed));
        }
    }
    return str;
}

static void check_bank(uint8_t *f, char tag, uint16_t seed)
{
    char want[16];
    int i;

    CHECK(D_8017A4E0.count0 == N0 && D_8017A4E0.count1 == N1 && D_8017A4E0.count2 == N2);
    CHECK(D_8017A4E0.count3 == N3 && D_8017A4E0.count4 == N4 && D_8017A4E0.count5 == N5);
    for (i = 0; i < N0; i++) {
        sprintf(want, "%cS%d", tag, i);
        CHECK(strcmp(D_8017A4E0.single[i], want) == 0);
    }
    for (i = 0; i < N2; i++) {
        sprintf(want, "%cA%d", tag, i);
        CHECK(strcmp(D_8017A4E0.taba[i], want) == 0);
    }
    sprintf(want, "%cB1", tag);
    CHECK(strcmp(D_8017A4E0.tab2b[1], want) == 0);
    CHECK(D_8017A4E0.tab[56] == 3 && D_8017A4E0.tab[2] == 6 + seed);
    CHECK(D_8017A4E0.tab2[2] == 6 + seed && D_8017A4E0.tab2a[0] == seed);
    CHECK((uint8_t *)D_8017A4E0.tab == f + 4 + N0 * 4 + 4);
    /* The file keeps its offsets: the pointers live in the native tables. */
    CHECK(pc_be32(f + 4) != 0 && *(uint32_t *)(f + 4) == (uint32_t)((uint8_t *)D_8017A4E0.single[0] - f));
}

int main(void)
{
    uint8_t *w = test_rdram();
    static uint8_t host[0x4000];
    uint8_t *f = w ? w + 0x100000 : host;   /* a heap block in the window when it can be mapped */
    size_t size;

    if (!w)
        fprintf(stderr, "test_text: no RDRAM window here, using host memory\n");
    g_bank = f;

    /* A bank that runs past its block is left as it is. */
    size = make_bank(f, 'e', 0);
    pc_assets_text(0, f, 64);
    CHECK(pc_be32(f) == N0);

    /* English, converted and parsed by the game. */
    pc_assets_text(0, f, 0x4000);
    {
        uint32_t n;
        uint16_t h;
        memcpy(&n, f, 4);
        CHECK(n == N0);
        memcpy(&h, f + 4 + N0 * 4 + 4 + 2 * 2, 2);
        CHECK(h == 6);
    }
    func_800A4E58();
    check_bank(f, 'e', 0);

    /* The readers. */
    D_801613E8 = 0;
    func_803AE51C(0);
    CHECK(g_drawn == D_8017A4E0.single[32]);
    CHECK((uint32_t)g_value == (uint32_t)(uintptr_t)D_8017A4E0.single[235]);
    D_80117350 = 1;
    voice_stop();
    CHECK(g_formatted == D_8017A4E0.single[233]);
    D_80117350 = 0;
    D_801170FC = 7;
    voice_stop();
    CHECK(g_measured == D_8017A4E0.taba[3 + 7]);
    D_801170FC = 5;
    {
        char name[32] = "....................PLAYER";
        void *p = name;
        memcpy(D_8014A160, &p, sizeof p);
    }
    voice_stop();
    CHECK(g_formatted == D_8017A4E0.taba[3]);

    /* The language switch: ReplaceWave reads another bank into the block, func_800A510C parses it again. */
    memset(f, 0, 0x4000);
    CHECK(make_bank(f, 'd', 5) <= 0x4000);
    pc_assets_text(3, f, 0x4000);
    func_800A4E58();
    check_bank(f, 'd', 5);
    D_80117350 = 1;
    voice_stop();
    CHECK(strcmp(g_formatted, "dS233") == 0);

    CHECK(pc_text_strings(1, NULL, 0, f) == NULL);
    (void)size;
    return TEST_DONE();
}
