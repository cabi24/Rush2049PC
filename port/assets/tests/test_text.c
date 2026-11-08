/* port/assets/tests/test_text.c: the text bank converter (port/assets/text.c) on synthetic big-endian banks, then
 * the game's own _AdjustTextWavePointers and three readers that view gText differently: by field
 * (titlescr_animText's table overlay through gText_single) and by raw N64 offsets (pause_TitlePane). A second bank in the same
 * block stands in for the language switch.
 * game: src/blob/groups/slot_sound/AdjustTextWavePointers.c src/ovl_a/titlescr_animText.c
 * game: src/blob/groups/pausescr_state/pause_TitlePane.c
 */
#include <stdarg.h>
#include <string.h>

#include "assets.h"
#include "support.h"

char **pc_text_strings(int section, const uint32_t *offsets, int count, void *base);

/* gText as the game's PC views see it (host layout of ModelTables), with gText_single (the single table) at the
 * address port/data gives it. */
struct TextHeader {
    uint16_t count0;
    char **single;
    uint16_t count1, count2;
    uint16_t *tab;
    char **taba;
    uint16_t count3, count4, count5;
    uint16_t *tab2, *tab2a;
    char **tab2b;
} gText;
__asm__(".globl gText_single\n.set gText_single, gText + 8\n");

/* Stand-ins for what the game files reference. */
static uint8_t *g_bank;
int32_t gTextWaveInst;
void *GetWaveInstanceAddr(int32_t index) { return g_bank; }

int8_t attract_alt_text_flag;
uint8_t titlescr_prompt_text[4];
static void *g_drawn;
static void *g_value;
void HAL_SetTextDepth(float v) {}
void TB_SetJust(int32_t a, int32_t b) {}
void HAL_SetFont(int32_t id) {}
void TB_SetTextColor(int32_t id) {}
void TB_DrawTextScrollBox(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color, int32_t flags, void *text) { g_drawn = text; }
void TB_DrawText(int16_t x, int16_t y, void *text) { g_value = text; }   /* TB_DrawText */

uint8_t text_lock_queue[24], pause_titlePos[4], gPlayers_save[76 * 2];
char pause_title_format[] = "%d %s", pause_title_name_format[] = "%s %s";
int32_t gPauseState, playerInPause;
int8_t pause_gControllerRemoved;
void *pause_gTitlePane;
static const char *g_formatted, *g_measured;
int32_t osRecvMesg(void *q, void *m, int32_t f) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t f) { return 0; }
int32_t HAL_SetFontInternal(int32_t font) { return 0; }
int16_t TB_GetFontLineHeight(void) { return 8; }
void *InitPane(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g, int32_t h) { return &g_bank; }
void ChangePaneSize(void *p, int32_t a, int32_t b, int32_t c, int32_t d) {}
int32_t HAL_GetTextWidth(uint8_t *s, int16_t max) { g_measured = (const char *)s; return 10; }
void TB_Sprintf(char *out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (fmt == pause_title_format)
        (void)va_arg(ap, int);
    else
        (void)va_arg(ap, char *);
    g_formatted = va_arg(ap, const char *);
    va_end(ap);
    strcpy(out, "x");
}

void AdjustTextWavePointers(void);
int32_t titlescr_animText(int32_t arg0);
void pause_TitlePane(void);

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

    CHECK(gText.count0 == N0 && gText.count1 == N1 && gText.count2 == N2);
    CHECK(gText.count3 == N3 && gText.count4 == N4 && gText.count5 == N5);
    for (i = 0; i < N0; i++) {
        sprintf(want, "%cS%d", tag, i);
        CHECK(strcmp(gText.single[i], want) == 0);
    }
    for (i = 0; i < N2; i++) {
        sprintf(want, "%cA%d", tag, i);
        CHECK(strcmp(gText.taba[i], want) == 0);
    }
    sprintf(want, "%cB1", tag);
    CHECK(strcmp(gText.tab2b[1], want) == 0);
    CHECK(gText.tab[56] == 3 && gText.tab[2] == 6 + seed);
    CHECK(gText.tab2[2] == 6 + seed && gText.tab2a[0] == seed);
    CHECK((uint8_t *)gText.tab == f + 4 + N0 * 4 + 4);
    /* The file keeps its offsets: the pointers live in the native tables. */
    CHECK(pc_be32(f + 4) != 0 && *(uint32_t *)(f + 4) == (uint32_t)((uint8_t *)gText.single[0] - f));
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
    AdjustTextWavePointers();
    check_bank(f, 'e', 0);

    /* The readers. */
    attract_alt_text_flag = 0;
    titlescr_animText(0);
    CHECK(g_drawn == gText.single[32]);
    CHECK(g_value == gText.single[235]);
    pause_gControllerRemoved = 1;
    pause_TitlePane();
    CHECK(g_formatted == gText.single[233]);
    pause_gControllerRemoved = 0;
    gPauseState = 7;
    pause_TitlePane();
    CHECK(g_measured == gText.taba[3 + 7]);
    gPauseState = 5;
    {
        static char name[32] = "....................PLAYER";   /* static: pause_TitlePane reads it after the block */
        void *p = name;
        memcpy(gPlayers_save, &p, sizeof p);
    }
    pause_TitlePane();
    CHECK(g_formatted == gText.taba[3]);

    /* The language switch: ReplaceWave reads another bank into the block, LoadTextWave parses it again. */
    memset(f, 0, 0x4000);
    CHECK(make_bank(f, 'd', 5) <= 0x4000);
    pc_assets_text(3, f, 0x4000);
    AdjustTextWavePointers();
    check_bank(f, 'd', 5);
    pause_gControllerRemoved = 1;
    pause_TitlePane();
    CHECK(strcmp(g_formatted, "dS233") == 0);

    CHECK(pc_text_strings(1, NULL, 0, f) == NULL);
    (void)size;
    return TEST_DONE();
}
