/* port/assets/tests/test_hudtext.c: two text callbacks of the game image, from their drafts in nonmatching/game:
 * animSpeedText, the HUD's digital speedometer, and func_8010E8B4 (animPanes@videoscr.c), the video screen's panes.
 * The speedometer reads the car records in their PC layout (0x468 bytes, docs/port/CARS.md) by the model records'
 * slots (N64 layout), and its units from the text bank's string table (a host pointer at +8, ModelTables); the
 * panes' InitPane (ambient_sound_set) takes a pointer-sized sixth argument. The test stands in for the font and
 * text functions and records what they draw.
 * game: nonmatching/game/animSpeedText.c nonmatching/game/func_8010E8B4.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define CF(c, off, T) (*(T *)((uint8_t *)(c) + CAR_OFS(off)))   /* a car field by its N64 offset */
#define NF(n, off, T) (*(T *)((uint8_t *)(n) + (off)))

/* ---------------------------------------------------------------- the game's data */

int8_t D_8015F72C, D_801613AA;                 /* gShowSpeed, gShowMetric */
struct { int32_t h, v; } D_80118E20;           /* gJust */
int16_t gNumWindows;
struct pc_model pc_models[PC_MODEL_SLOTS];
__attribute__((aligned(16))) uint8_t D_80152818[6 * PC_CAR_SIZE];
#define CAR(i) (D_80152818 + PC_CAR_SIZE * (i))
#define MODEL(i) ((uint8_t *)&pc_models[i])
int32_t D_80115A68[4][4][2];                   /* speedPos */
/* The text bank (ModelTables, showhud.c): its first string table at +8. */
static char *g_strings[300];
struct {
    uint16_t count0; char **ptrs0; uint16_t count1, count2; uint16_t *data1; char **ptrs2;
    uint16_t count3, count4, count5; uint16_t *data3, *data4; char **ptrs5;
} D_8017A4E0 = {.count0 = 300, .ptrs0 = g_strings};
void *D_801497F0;                              /* gFontHeader (only the unused private copies read it) */

int8_t D_8011AD38, D_8011AD6C;
uint32_t D_801174B4;
int32_t D_8015698C;
int8_t gControllerData[4][16];
int32_t D_8011AD70[4];                         /* the menu's rectangle: x, w, h, cx */

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
void render_helper(float depth) { g_depth = depth; }
int32_t object_create(int32_t font) { int32_t old = g_font; g_font = font; record('F')->a = font; return old; }
int32_t HAL_GetCharWidth(uint16_t ch) { return ch == '8' ? 7 : 5; }
void dispatch_handler(int32_t color) { g_color = color; }
void state_utility(int16_t x, int16_t y, void *text)
{
    Call *c = record('T');
    c->a = x, c->b = y;
    strncpy(c->text, text, sizeof c->text - 1);
}
int16_t object_bytes23_sum(void) { return 10; }
int16_t object_bytes_sum_global(void) { return 12; }
void sound_update_channel(int32_t force) {}
static int g_titles;
void TitlePane(void) { g_titles++; }
static int g_bad_word40;
void *ambient_sound_set(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t style, intptr_t word40, int32_t word44,
                        int32_t own)
{
    Call *c = record('P');
    c->a = x1, c->b = y1, c->c = x2, c->d = y2, c->e = style;
    if (word40 != 0 || word44 != 0 || own != 0)
        g_bad_word40++;
    return NULL;
}

int32_t animSpeedText(void *blt);
int32_t func_8010E8B4(void *item);

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
    D_8015F72C = 0, g_ncalls = 0;
    CHECK(animSpeedText(NULL) == 1 && g_ncalls == 0);

    D_8015F72C = 1, gNumWindows = 2, D_801613AA = 0;
    NF(MODEL(0), 0x7C6, int16_t) = 2, NF(MODEL(1), 0x7C6, int16_t) = 0;
    CF(CAR(2), 0xEF, int8_t) = 1;                    /* car 2 (window 0's) is locked in place */
    CF(CAR(1), 0xF8, int16_t) = 200;                 /* window 1 reads car 1: 200 >> 2 = 50, x 1.2 = 60 */
    CF(CAR(0), 0xF8, int16_t) = 999;
    D_80115A68[1][0][0] = 10, D_80115A68[1][0][1] = 20;
    D_80115A68[1][1][0] = 100, D_80115A68[1][1][1] = 50;
    g_ncalls = 0;
    CHECK(animSpeedText(NULL) == 1);
    CHECK(g_calls[0].what == 'F' && g_calls[0].a == 1 && g_depth == -1.0f);   /* font 1 for two windows */
    i = find('T', 0);
    CHECK(text_at(i, 92, 50, "8", 0) && text_at(i + 1, 92, 50, "0", 14));      /* x - (width of '8' + 1) */
    CHECK(text_at(i + 2, 100, 50, "8", 0) && text_at(i + 3, 100, 50, "6", 14));
    CHECK(text_at(i + 4, 108, 50, "8", 0) && text_at(i + 5, 108, 50, "0", 14));
    CHECK(g_calls[i + 6].what == 'F' && g_calls[i + 6].a == 12);               /* the units' font */
    CHECK(text_at(i + 7, 101, 57, "MPH", 0) && text_at(i + 8, 100, 56, "MPH", 1));   /* y + font height - 4 */
    CHECK(g_ncalls == i + 9 && D_80118E20.h == 0 && D_80118E20.v == 3);

    /* metric, and a negative speed: |-100 x 1.2 x 1.61| = 193 */
    D_801613AA = 1, CF(CAR(1), 0xF8, int16_t) = -400, g_ncalls = 0;
    CHECK(animSpeedText(NULL) == 1);
    i = find('T', 0);
    CHECK(text_at(i + 1, 92, 50, "1", 14) && text_at(i + 3, 100, 50, "9", 14) && text_at(i + 5, 108, 50, "3", 14));
    CHECK(text_at(i + 7, 101, 57, "KPH", 0) && text_at(i + 8, 100, 56, "KPH", 1));

    /* one window: font 4 */
    gNumWindows = 1, CF(CAR(2), 0xEF, int8_t) = 0, NF(MODEL(0), 0x7C6, int16_t) = 0, g_ncalls = 0;
    D_80115A68[0][0][0] = 200, D_80115A68[0][0][1] = 30, CF(CAR(0), 0xF8, int16_t) = 40;   /* 10 x 1.2 = 12 */
    D_801613AA = 0;
    CHECK(animSpeedText(NULL) == 1);
    i = find('T', 0);
    CHECK(g_calls[0].a == 4 && g_calls[i - 1].what == 'F' && g_calls[i - 1].a == 4);
    CHECK(text_at(i + 1, 192, 30, "0", 14) && text_at(i + 3, 200, 30, "1", 14) && text_at(i + 5, 208, 30, "2", 14));
}

/* The video screen's panes: in a game (no front-end state), the title and the menu's frame once; the box always. */
static void test_panes(void)
{
    int i;

    D_8011AD38 = 0, D_801174B4 = 0, D_8015698C = 2, g_titles = 0, g_bad_word40 = 0, g_ncalls = 0;
    gControllerData[2][0] = 0, gControllerData[0][0] = 1;
    D_8011AD70[0] = 40, D_8011AD70[1] = 200, D_8011AD70[2] = 60;
    CHECK(func_8010E8B4(NULL) == 1);
    CHECK(g_calls[0].what == 'F' && g_calls[0].a == 13 && D_8011AD6C == 1 && g_titles == 1 && D_8011AD38 == 1);
    i = find('P', 0);   /* y = line height x 2 + 10 = 34 */
    CHECK(i == 1 && g_calls[i].a == 32 && g_calls[i].b == 30 && g_calls[i].c == 248 && g_calls[i].d == 98);
    CHECK(g_calls[i].e == 176 && g_calls[i + 1].what == 'P' && g_calls[i + 1].a == 20 && g_calls[i + 1].b == 136);
    CHECK(g_calls[i + 1].c == 300 && g_calls[i + 1].d == 204 && g_ncalls == 3 && g_bad_word40 == 0);

    g_ncalls = 0;   /* once */
    CHECK(func_8010E8B4(NULL) == 1 && g_ncalls == 0);

    /* in the front end: only the box; a controller in the port: not removed */
    D_8011AD38 = 0, D_801174B4 = 2, gControllerData[2][0] = 1, D_8011AD6C = 7;
    CHECK(func_8010E8B4(NULL) == 1);
    CHECK(g_ncalls == 1 && g_calls[0].what == 'P' && g_calls[0].a == 20 && g_titles == 1 && D_8011AD6C == 7);
    D_8011AD38 = 0, D_801174B4 = 0, g_ncalls = 0;
    CHECK(func_8010E8B4(NULL) == 1 && D_8011AD6C == 0);
}

int main(void)
{
    test_speed();
    test_panes();
    return TEST_DONE();
}
