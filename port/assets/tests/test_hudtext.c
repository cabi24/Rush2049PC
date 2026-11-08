/* port/assets/tests/test_hudtext.c: two text callbacks of the game image, from their drafts in nonmatching/game:
 * animSpeedText, the HUD's digital speedometer, and video_animPanes (animPanes@videoscr.c), the video screen's panes.
 * The speedometer reads the car records in their PC layout (0x468 bytes, docs/port/CARS.md) by the model records'
 * slots (N64 layout), and its units from the text bank's string table (a host pointer at +8, ModelTables); the
 * panes' InitPane takes a pointer-sized sixth argument. The test stands in for the font and
 * text functions and records what they draw.
 * game: nonmatching/game/animSpeedText.c nonmatching/game/video_animPanes.c src/blob/TB_SetJust.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define CF(c, off, T) (*(T *)((uint8_t *)(c) + CAR_OFS(off)))   /* a car field by its N64 offset */
#define NF(n, off, T) (*(T *)((uint8_t *)(n) + (off)))

/* ---------------------------------------------------------------- the game's data */

int8_t gShowSpeed, gShowMetric;                 /* gShowSpeed, gShowMetric */
struct { int32_t h, v; } gJust;
int16_t gNumWindows;
struct pc_model pc_models[PC_MODEL_SLOTS];
struct pc_car pc_cars[PC_CAR_SLOTS];                  /* the car records (pc_game_types.h) */
#define CAR(i) ((uint8_t *)&pc_cars[i])
#define MODEL(i) ((uint8_t *)&pc_models[i])
int32_t speedPos[4][4][2];                   /* speedPos */
/* The text bank (ModelTables, showhud.c): its first string table at +8. */
static char *g_strings[300];
struct {
    uint16_t count0; char **ptrs0; uint16_t count1, count2; uint16_t *data1; char **ptrs2;
    uint16_t count3, count4, count5; uint16_t *data3, *data4; char **ptrs5;
} gText = {.count0 = 300, .ptrs0 = g_strings};
void *gFontHeader;                              /* gFontHeader (only the unused private copies read it) */

int8_t video_panesCreated, video_gControllerRemoved;
uint32_t gGameState;
int32_t playerInPause;
int8_t gControllerData[4][16];
int32_t video_menuRect[4];                         /* the menu's rectangle: x, w, h, cx */

/* ---------------------------------------------------------------- what the code under test calls */

typedef struct { char what; int32_t a, b, c, d, e, color; char text[16]; } Call;
static Call g_calls[64];
static int g_ncalls, g_color = -1, g_font = -1;
static float g_depth;
static Call *record(char what)
{
    Call *c = &g_calls[g_ncalls < 64 ? g_ncalls++ : 63];
    memset(c, 0, sizeof *c);
    c->what = what, c->color = g_color;
    return c;
}
void HAL_SetTextDepth(float depth) { g_depth = depth; }
int32_t HAL_SetFont(int32_t font) { int32_t old = g_font; g_font = font; record('F')->a = font; return old; }
int32_t HAL_GetCharWidth(uint16_t ch) { return ch == '8' ? 7 : 5; }
void TB_SetTextColor(int32_t color) { g_color = color; }
void TB_DrawText(int16_t x, int16_t y, void *text)
{
    Call *c = record('T');
    c->a = x, c->b = y;
    strncpy(c->text, text, sizeof c->text - 1);
}
int16_t TB_GetFontHeight(void) { return 10; }
int16_t TB_GetFontLineHeight(void) { return 12; }
void SetupFontData(int32_t force) {}
static int g_titles;
void video_TitlePane(void) { g_titles++; }
static int g_bad_word40;
void *InitPane(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t style, intptr_t word40, int32_t word44,
                        int32_t own)
{
    Call *c = record('P');
    c->a = x1, c->b = y1, c->c = x2, c->d = y2, c->e = style;
    if (word40 != 0 || word44 != 0 || own != 0)
        g_bad_word40++;
    return NULL;
}

int32_t animSpeedText(void *blt);
int32_t video_animPanes(void *item);

/* ---------------------------------------------------------------- tests */

static int find(char what, int from)
{
    for (int i = from; i < g_ncalls; i++)
        if (g_calls[i].what == what)
            return i;
    return -1;
}

static int text_at(int i, int x, int y, const char *s, int color)
{
    return i >= 0 && i < g_ncalls && g_calls[i].what == 'T' && g_calls[i].a == x && g_calls[i].b == y &&
           !strcmp(g_calls[i].text, s) && g_calls[i].color == color;
}

/* The speedometer: window 0's car is locked in place (skipped), window 1 shows car 1's speed in MPH. */
static void test_speed(void)
{
    int i;

    g_strings[202] = "MPH", g_strings[203] = "KPH";
    gShowSpeed = 0, g_ncalls = 0;
    CHECK(animSpeedText(NULL) == 1 && g_ncalls == 0);

    gShowSpeed = 1, gNumWindows = 2, gShowMetric = 0;
    NF(MODEL(0), 0x7C6, int16_t) = 2, NF(MODEL(1), 0x7C6, int16_t) = 0;
    CF(CAR(2), 0xEF, int8_t) = 1;                    /* car 2 (window 0's) is locked in place */
    CF(CAR(1), 0xF8, int16_t) = 200;                 /* window 1 reads car 1: 200 >> 2 = 50, x 1.2 = 60 */
    CF(CAR(0), 0xF8, int16_t) = 999;
    speedPos[1][0][0] = 10, speedPos[1][0][1] = 20;
    speedPos[1][1][0] = 100, speedPos[1][1][1] = 50;
    g_ncalls = 0;
    CHECK(animSpeedText(NULL) == 1);
    CHECK(g_calls[0].what == 'F' && g_calls[0].a == 1 && g_depth == -1.0f);   /* font 1 for two windows */
    i = find('T', 0);
    CHECK(text_at(i, 92, 50, "8", 0) && text_at(i + 1, 92, 50, "0", 14));      /* x - (width of '8' + 1) */
    CHECK(text_at(i + 2, 100, 50, "8", 0) && text_at(i + 3, 100, 50, "6", 14));
    CHECK(text_at(i + 4, 108, 50, "8", 0) && text_at(i + 5, 108, 50, "0", 14));
    CHECK(g_calls[i + 6].what == 'F' && g_calls[i + 6].a == 12);               /* the units' font */
    CHECK(text_at(i + 7, 101, 57, "MPH", 0) && text_at(i + 8, 100, 56, "MPH", 1));   /* y + font height - 4 */
    CHECK(g_ncalls == i + 9 && gJust.h == 0 && gJust.v == 3);

    /* metric, and a negative speed: |-100 x 1.2 x 1.61| = 193 */
    gShowMetric = 1, CF(CAR(1), 0xF8, int16_t) = -400, g_ncalls = 0;
    CHECK(animSpeedText(NULL) == 1);
    i = find('T', 0);
    CHECK(text_at(i + 1, 92, 50, "1", 14) && text_at(i + 3, 100, 50, "9", 14) && text_at(i + 5, 108, 50, "3", 14));
    CHECK(text_at(i + 7, 101, 57, "KPH", 0) && text_at(i + 8, 100, 56, "KPH", 1));

    /* one window: font 4 */
    gNumWindows = 1, CF(CAR(2), 0xEF, int8_t) = 0, NF(MODEL(0), 0x7C6, int16_t) = 0, g_ncalls = 0;
    speedPos[0][0][0] = 200, speedPos[0][0][1] = 30, CF(CAR(0), 0xF8, int16_t) = 40;   /* 10 x 1.2 = 12 */
    gShowMetric = 0;
    CHECK(animSpeedText(NULL) == 1);
    i = find('T', 0);
    CHECK(g_calls[0].a == 4 && g_calls[i - 1].what == 'F' && g_calls[i - 1].a == 4);
    CHECK(text_at(i + 1, 192, 30, "0", 14) && text_at(i + 3, 200, 30, "1", 14) && text_at(i + 5, 208, 30, "2", 14));
}

/* The video screen's panes: in a game (no front-end state), the title and the menu's frame once; the box always. */
static void test_panes(void)
{
    int i;

    video_panesCreated = 0, gGameState = 0, playerInPause = 2, g_titles = 0, g_bad_word40 = 0, g_ncalls = 0;
    gControllerData[2][0] = 0, gControllerData[0][0] = 1;
    video_menuRect[0] = 40, video_menuRect[1] = 200, video_menuRect[2] = 60;
    CHECK(video_animPanes(NULL) == 1);
    CHECK(g_calls[0].what == 'F' && g_calls[0].a == 13 && video_gControllerRemoved == 1 && g_titles == 1 && video_panesCreated == 1);
    i = find('P', 0);   /* y = line height x 2 + 10 = 34 */
    CHECK(i == 1 && g_calls[i].a == 32 && g_calls[i].b == 30 && g_calls[i].c == 248 && g_calls[i].d == 98);
    CHECK(g_calls[i].e == 176 && g_calls[i + 1].what == 'P' && g_calls[i + 1].a == 20 && g_calls[i + 1].b == 136);
    CHECK(g_calls[i + 1].c == 300 && g_calls[i + 1].d == 204 && g_ncalls == 3 && g_bad_word40 == 0);

    g_ncalls = 0;   /* once */
    CHECK(video_animPanes(NULL) == 1 && g_ncalls == 0);

    /* in the front end: only the box; a controller in the port: not removed */
    video_panesCreated = 0, gGameState = 2, gControllerData[2][0] = 1, video_gControllerRemoved = 7;
    CHECK(video_animPanes(NULL) == 1);
    CHECK(g_ncalls == 1 && g_calls[0].what == 'P' && g_calls[0].a == 20 && g_titles == 1 && video_gControllerRemoved == 7);
    video_panesCreated = 0, gGameState = 0, g_ncalls = 0;
    CHECK(video_animPanes(NULL) == 1 && video_gControllerRemoved == 0);
}

int main(void)
{
    test_speed();
    test_panes();
    return TEST_DONE();
}
