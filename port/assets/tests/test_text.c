/* port/assets/tests/test_text.c: the text bank converter (port/assets/text.c) on synthetic big-endian banks: the
 * bank's records (its struct TextTables, built beside the file), the game's own AdjustTextWavePointers, which copies
 * them into gText, and two readers (titlescr_animText's strings by index, pause_TitlePane's tables). A second bank in
 * the same block stands in for the language switch (its string tables stay where they were), a larger one moves them,
 * and the records go with the block.
 * game: src/blob/groups/slot_sound/AdjustTextWavePointers.c src/ovl_a/titlescr_animText.c
 * game: src/blob/groups/pausescr_state/pause_TitlePane.c
 */
#include <stdarg.h>
#include <string.h>

#include "assets.h"
#include "pc_game_types.h"
#include "support.h"

/* The game's gText: its one declaration (pc_game_types.h). */
struct TextTables gText;

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

/* Section sizes of the synthetic banks; a long bank has MORE single strings than the others. */
enum { N0 = 240, N1 = 60, N2 = 12, N3 = 3, N4 = 1, N5 = 2, MORE = 30 };

/* Writes a big-endian bank of n0 single strings: string offsets point to "<tag><section letter><index>",
 * tab[i] = i * 3 + seed, tab[56] = 3. Returns the file size. */
static size_t make_bank(uint8_t *f, char tag, uint16_t seed, int n0)
{
    const int counts[6] = {n0, N1, N2, N3, N4, N5};
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

static void check_bank(uint8_t *f, char tag, uint16_t seed, int n0)
{
    char want[16];
    int i;

    CHECK(gText.numSingle == n0 && gText.numTab == N1 && gText.numTabA == N2);
    CHECK(gText.numTab2 == N3 && gText.numTab2a == N4 && gText.numTab2b == N5);
    for (i = 0; i < n0; i++) {
        sprintf(want, "%cS%d", tag, i);
        CHECK(strcmp((char *)gText.single[i], want) == 0);
    }
    for (i = 0; i < N2; i++) {
        sprintf(want, "%cA%d", tag, i);
        CHECK(strcmp((char *)gText.taba[i], want) == 0);
    }
    sprintf(want, "%cB1", tag);
    CHECK(strcmp((char *)gText.tab2b[1], want) == 0);
    CHECK(gText.tab[56] == 3 && gText.tab[2] == 6 + seed);
    CHECK(gText.tab2[2] == 6 + seed && gText.tab2a[0] == seed);
    CHECK((uint8_t *)gText.tab == f + 4 + n0 * 4 + 4);
    /* The file keeps its offsets: the pointers live in the bank's records. */
    CHECK(pc_be32(f + 4) != 0 && *(uint32_t *)(f + 4) == (uint32_t)((uint8_t *)gText.single[0] - f));
}

int main(void)
{
    static uint8_t f[0x4000];   /* the bank's heap block */
    unsigned char **single, **taba;

    g_bank = f;

    /* A bank that runs past its block is left as it is, with no tables. */
    make_bank(f, 'e', 0, N0);
    pc_assets_text(0, f, 64);
    CHECK(pc_be32(f) == N0);
    CHECK(pc_text_bank(f)->numSingle == 0 && !pc_text_bank(f)->single);

    /* English, converted, and its tables taken by the game. */
    pc_assets_text(0, f, sizeof f);
    {
        uint32_t n;
        uint16_t h;
        memcpy(&n, f, 4);
        CHECK(n == N0);
        memcpy(&h, f + 4 + N0 * 4 + 4 + 2 * 2, 2);
        CHECK(h == 6);
    }
    AdjustTextWavePointers();
    check_bank(f, 'e', 0, N0);
    CHECK(memcmp(&gText, pc_text_bank(f), sizeof gText) == 0);
    single = gText.single;
    taba = gText.taba;

    /* The readers. */
    attract_alt_text_flag = 0;
    titlescr_animText(0);
    CHECK(g_drawn == gText.single[32]);
    CHECK(g_value == gText.single[235]);
    pause_gControllerRemoved = 1;
    pause_TitlePane();
    CHECK(g_formatted == (char *)gText.single[233]);
    pause_gControllerRemoved = 0;
    gPauseState = 7;
    pause_TitlePane();
    CHECK(g_measured == (char *)gText.taba[3 + 7]);
    gPauseState = 5;
    {
        static char name[32] = "....................PLAYER";   /* static: pause_TitlePane reads it after the block */
        void *p = name;
        memcpy(gPlayers_save, &p, sizeof p);
    }
    pause_TitlePane();
    CHECK(g_formatted == (char *)gText.taba[3]);

    /* The language switch: ReplaceWave reads another bank into the block, LoadTextWave takes its tables again. They
     * are where the last language's were, so gText's tables never point at freed memory between the two. */
    memset(f, 0, sizeof f);
    CHECK(make_bank(f, 'd', 5, N0) <= sizeof f);
    pc_assets_text(3, f, sizeof f);
    CHECK(pc_text_bank(f)->single == single && pc_text_bank(f)->taba == taba);
    AdjustTextWavePointers();
    check_bank(f, 'd', 5, N0);
    pause_gControllerRemoved = 1;
    pause_TitlePane();
    CHECK(strcmp(g_formatted, "dS233") == 0);

    /* A language with more strings than the tables have room for. */
    memset(f, 0, sizeof f);
    CHECK(make_bank(f, 'f', 2, N0 + MORE) <= sizeof f);
    pc_assets_text(4, f, sizeof f);
    AdjustTextWavePointers();
    check_bank(f, 'f', 2, N0 + MORE);

    /* The records go with the block: when the heap frees it, or another file is loaded over part of it. */
    pc_heap_block_freed(f, sizeof f);
    CHECK(pc_text_bank(f)->numSingle == 0);
    memset(f, 0, sizeof f);
    make_bank(f, 'g', 1, N0);
    pc_assets_text(1, f, sizeof f);
    CHECK(pc_text_bank(f)->numSingle == N0);
    pc_records_keep(f + 0x100, 0x100, PC_RECORDS_PATH, NULL, NULL);
    CHECK(pc_text_bank(f)->numSingle == 0);
    pc_records_drop(f, sizeof f);
    return TEST_DONE();
}
