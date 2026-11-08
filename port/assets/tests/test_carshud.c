/* port/assets/tests/test_carshud.c: the HUD for races of more than 6 cars (docs/port/MORECARS.md 7), on the game's own
 * code. animPlace's "P/N" with numbers of two digits, and its N64 calls for a race of up to 9 cars; the HUD order
 * (hud_updateFrame) for 12 cars, cleared over its 12 entries above 6 cars and over the N64's 6 at 6; ShowHUD's race set
 * (ShowHUD) the ROM's own up to 8 cars, and above 8 a copy with each run of minimap dots and of a viewer's
 * radar blips continued after its 7 up to the race's cars; the dots (animDot) and the blips (animRadarBlip) for
 * slots 8-11 drawing a human's and a drone's icon; the results' finishing place (drawRaceStandings) for places 7-12,
 * the number and what the bank's sixth ordinal has after its own. The race's count is the asset tests' latch
 * (support.c: test_race_cars, pc_race_cars_begin). The test stands in for the font, text, blit and sprite functions
 * and records what they are given. The values and the strings are made up.
 * game: src/blob/groups/hud_text_anims/animPlace.c src/blob/hud_updateFrame.c src/blob/groups/showhud/showhud.c
 * game: src/blob/animDot.c src/blob/animRadarBlip.c src/blob/groups/gameoverscr_draw/results_table.c
 * game: src/blob/animPause.c
 */
#include <stdarg.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* NewMultiBlit.c's Blit: the layout every PC view has (test_hudblit.c). */
typedef struct Blit {
    const char *Name;
    void *Image, *Info;
    uint16_t TexIndex;
    int16_t X, Y;
    uint16_t Z;
    int16_t Width, Height;
    uint8_t Alpha, Flip;
    int8_t Hide, Init;
    int16_t Top, Bot, Left, Right, color;
    uint16_t reserved;
    int32_t (*AnimFunc)(struct Blit *);
    uint32_t AnimID;
    intptr_t AnimDTA;
    uint16_t BLIdx, reserved36;
    void *data;
    struct Blit *child;
} Blit;
_Static_assert(sizeof(Blit) == 0x68 && offsetof(Blit, AnimID) == 0x40, "the Blit's host layout");

/* ---------------------------------------------------------------- the game's data */

struct pc_model pc_models[PC_MODEL_SLOTS];
struct pc_car pc_cars[PC_CAR_SLOTS];
struct { uint32_t opaque[6]; } text_lock_queue;     /* the font lock */
int32_t gGameState, gRace;
volatile int16_t gNumCars;
int8_t gShowPlace;
int16_t gNumWindows, gNumPlayers;
int32_t gJust[2];                         /* gJust */
struct { int32_t x, y; } placePos[4][4];     /* placePos [windows - 1][player] */
uint8_t hud_playerColor[4][4];                      /* playerColor */
/* hud_updateFrame */
/* the input records (gPlayers, defined with the results' view below): each human's slot */
uint8_t TrackTimeStr[6][8];
float gModelTime;
int8_t dot_order_list[16];                         /* the HUD order: 12 in the arena, 4 more here to see past them */
int8_t gNumActiveCars;
volatile int16_t num_humans;
int16_t markers_valid;
/* ShowHUD */
void *HudBlit1, *HudBlit2, *multiPlayerBlit;
struct { int16_t id; uint8_t pad[50]; } gPlayerCoin[4][2];
float gCheckPtTimeout[4], gLapTime[4], gElapsedTime[4];
int8_t expansion_pak_present, gTrackNum;
int16_t gNumWindows;
struct pc_multiblit OverlayMultiBlit[1], TwoPlayerMultiBlit[1], ThreePlayerMultiBlit[1], D_80114C58[89], HudMultiBlit[8];
int32_t gShowCredits, gCredit, gCorner, gCreditWidth, gNumCreditNames;
int32_t pauseMenuSelect; uint32_t animPause_color;       /* animPause.c's, where Hidden is */
volatile float gSeconds;
char *cStrCreditNames[4][11];
/* gText: defined with the results' view below */
/* the dots and the blips */
int8_t gShowTrackMap, gMirrorWaves, gShowRadar;     /* the map, mirrored, the radar */
struct { int16_t x, y, z; } gPathMax, gPathMin;   /* the track's bounds */
int32_t kMapSize, radarWidth, radarHeight;    /* the map's size, the radar's */
struct { int32_t x, y; } hud_mapPos[4], radarPos[4][4];
struct { void *pal, *tex; uint16_t model; int16_t x, y; uint16_t mode; int16_t w, h; uint8_t color, flags; int8_t hidden;
         uint8_t pad17; int16_t t0, s0, t1, s1; } gBlitInstanceList[8];
struct { uint8_t pad[636]; uint32_t tick; } gScheduler;
/* drawRaceStandings and the rest of its file */
uint64_t g_dl[2 * 512];
void *blit_dl = g_dl;                       /* the display list */
int32_t blitMode, gMainSelect, gGameOverState, playerTextColor[8];
int8_t gRankedPlayer[8], gCarTeam[8], gBattlePoints[4][4];
/* the input records: each human's slot (+0, the results' vehicle) and profile name (+0x48) */
struct { uint8_t slot; uint8_t other[71]; char **name; } gPlayers[4];
struct { int32_t words[18]; int16_t laps[24]; } gStuntPlayer[4];
uint8_t gameover_titlePos[16], gCircuitPlayer, gLap[8];
int16_t gNumLaps;
float gBestLapTime[8], best_lap_time_by_column[8], gLapTimes[4][8];
char battle_score_total_format[8] = "%d", battle_score_cell_format[8] = "%d", stunt_score_total_format[8] = "%d", stunt_score_best_format[8] = "%d", stunt_score_count_format[8] = "%d",
     battle_standings_drone_name[8] = "%d", battle_standings_total_format[8] = "%d", stunt_standings_drone_name[8] = "%d", stunt_standings_points_format[8] = "%d", stunt_standings_best_format[8] = "%d",
     race_standings_drone_name[8] = "DRONE", race_standings_time_format[16] = "%d:%02d.%03d", race_standings_lap_format[16] = "%d:%02d.%03d",
     race_score_time_format[16] = "%d:%02d.%03d", race_score_best_lap_format[16] = "%d", race_score_lap_time_format[16] = "%d";
/* the text bank: the place strings from g_first, in results_table.c's view (TextState) */
static char *g_fixed[300], *g_strings[400];
static uint16_t g_indices[64];
/* the text bank (ModelTables' host layout): the credits' view and results_table.c's TextState (fixed = ptrs0,
 * indices = data1, strings = ptrs2) */
struct {
    uint16_t count0; char **ptrs0; uint16_t count1, count2; uint16_t *data1; char **ptrs2;
    uint16_t count3, count4, count5; uint16_t *data3, *data4; char **ptrs5;
} gText = {0, g_fixed, 0, 0, g_indices, g_strings};
enum { g_first = 350 };

/* ---------------------------------------------------------------- what the code under test calls */

typedef struct { char what; int32_t a, b, c, d, font, color; char text[24]; } Call;
static Call g_calls[256];
static int g_ncalls, g_color = -1, g_font = -1;
static Call *record(char what)
{
    Call *c = &g_calls[g_ncalls < 256 ? g_ncalls++ : 255];
    memset(c, 0, sizeof *c);
    c->what = what, c->color = g_color, c->font = g_font;
    return c;
}
int32_t osRecvMesg(void *q, void **m, int32_t flags) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t flags) { return 0; }
int32_t HAL_SetFontInternal(int32_t font) { int32_t old = g_font; g_font = font; return old; }   /* HAL_SetFont */
void HAL_SetTextDepth(float depth) {}
/* a digit's width in each font: 8 the big one, 5 and 6 the small ones */
int32_t HAL_GetCharWidth(uint16_t ch) { record('W')->a = ch; return g_font == 8 ? 12 : g_font == 5 ? 8 : 6; }
void TB_SetTextColor(int32_t color) { g_color = color; }
void HAL_SetTextColor(int32_t r, int32_t g, int32_t b, int32_t a) { g_color = 100 + r; }   /* the player's colour */
void TB_DrawText(int16_t x, int16_t y, char *text)
{
    Call *c = record('T');
    c->a = x, c->b = y;
    strncpy(c->text, text, sizeof c->text - 1);
}
int32_t AreWeSolid(int16_t player) { return 1; }   /* AreWeSolid */
int32_t cvt_time(float value, uint8_t *digits, uint8_t format) { return 0; }
/* ShowHUD's NewMultiBlit: the table and count it is given, and a copy of the table */
static const struct pc_multiblit *g_set;
static int g_set_n;
static struct pc_multiblit g_set_copy[160];
static Blit g_voice;
void *NewMultiBlit(int16_t x, int16_t y, const struct pc_multiblit *table, int16_t n)
{
    if (table != HudMultiBlit) {
        g_set = table, g_set_n = n;
        memcpy(g_set_copy, table, sizeof *table * (n < 160 ? n : 160));
    }
    return &g_voice;
}
void *InitPane(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t style, intptr_t a, int32_t b,
                        int32_t c) { return NULL; }
void RemovePanes(int32_t all) {}
void SetCountdownTimer(float t) {}
float GetCountdownTimeRemaining(void) { return 0; }
void ShowHUDExtra(void) {}
uint32_t HAL_GetTextWidth(void *text, int16_t n) { return 0; }
float Random(float f) { return 0; }
void UpdateBlit(Blit *b) {}   /* UpdateBlit */
void SelectBlit(Blit *b, int32_t icon, int32_t w, int32_t h) { Call *c = record('I'); c->a = icon, c->b = b->AnimID; }
void BodyVector(float *in, float *out, float *uvs) { memcpy(out, in, 3 * sizeof *out); }   /* frwtobod: no turn */
void TB_EllipseText(char *out, char *text, int32_t w) { strcpy(out, text); }
int32_t TB_Sprintf(char *out, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int32_t n = vsprintf(out, fmt, ap);
    va_end(ap);
    return n;
}
int32_t gameover_validOption(int32_t i) { return 0; }
int32_t TB_GetFontLineHeight(void) { return 0; }

int32_t animPlace(int32_t arg0);
int32_t hud_updateFrame(int32_t arg0);
void ShowHUD(void);
int32_t animDot(Blit *b);
int32_t animRadarBlip(Blit *b);
void drawRaceStandings(void);

/* ---------------------------------------------------------------- the place, "P/N" */

static int text_at(int i, int x, int y, const char *s, int font, int color)
{
    return i >= 0 && i < g_ncalls && g_calls[i].what == 'T' && g_calls[i].a == x && g_calls[i].b == y &&
           !strcmp(g_calls[i].text, s) && g_calls[i].font == font && g_calls[i].color == color;
}

/* Glyph k of "P/N" drawn at x: its shadow at (x + 1, y + 1) in colour 0, then itself, the place's in the player's
 * colour (player 0's red is 200: 300) and the rest in colour 1. */
static int glyph(int i, int x, int y, const char *s, int font, int place)
{
    return text_at(i, x + 1, y + 1, s, font, 0) && text_at(i + 1, x, y, s, font, place ? 300 : 1);
}

static int count(char what)
{
    int n = 0;
    for (int i = 0; i < g_ncalls; i++)
        n += g_calls[i].what == what;
    return n;
}

static void test_place(void)
{
    gGameState = 0, gRace = 0, gShowPlace = 1, gNumWindows = 1;
    placePos[0][0].x = 250, placePos[0][0].y = 20;
    hud_playerColor[0][0] = 200;
    memset(pc_cars, 0, sizeof pc_cars);

    /* 6 cars, sixth: the N64's "6/6", the place one digit left of its point, '/' at it, the count right of it in the
     * small font; one width measured (the big font's '8') */
    gNumCars = 6, pc_cars[0].place = 5, g_ncalls = 0;
    CHECK(animPlace(0) == 1);
    CHECK(g_ncalls == 7 && g_calls[0].what == 'W' && g_calls[0].a == '8' && g_calls[0].font == 8);
    CHECK(glyph(1, 238, 20, "6", 8, 1) && glyph(3, 250, 20, "/", 8, 0) && glyph(5, 262, 20, "6", 5, 0));

    /* 9 cars, first: one digit each still */
    gNumCars = 9, pc_cars[0].place = 0, g_ncalls = 0;
    animPlace(0);
    CHECK(g_ncalls == 7 && glyph(1, 238, 20, "1", 8, 1) && glyph(3, 250, 20, "/", 8, 0) && glyph(5, 262, 20, "9", 5, 0));

    /* 10 cars, ninth: "9/10": the count's second digit one small digit on (the small font's width measured) */
    gNumCars = 10, pc_cars[0].place = 8, g_ncalls = 0;
    animPlace(0);
    CHECK(g_ncalls == 10 && glyph(1, 238, 20, "9", 8, 1) && glyph(3, 250, 20, "/", 8, 0));
    CHECK(g_calls[5].what == 'W' && g_calls[5].font == 5 && glyph(6, 262, 20, "1", 5, 0) && glyph(8, 270, 20, "0", 5, 0));

    /* 12 cars, twelfth: "12/12": the place's two digits, both in the player's colour, end where the one did */
    gNumCars = 12, pc_cars[0].place = 11, g_ncalls = 0;
    animPlace(0);
    CHECK(g_ncalls == 12 && glyph(1, 226, 20, "1", 8, 1) && glyph(3, 238, 20, "2", 8, 1));
    CHECK(glyph(5, 250, 20, "/", 8, 0) && g_calls[7].what == 'W' && glyph(8, 262, 20, "1", 5, 0));
    CHECK(glyph(10, 270, 20, "2", 5, 0));
    CHECK(count('T') == 10);   /* no ':', ';' or '<' */
    for (int i = 0; i < g_ncalls; i++)
        CHECK(g_calls[i].what != 'T' || (g_calls[i].text[0] >= '0' && g_calls[i].text[0] <= '9') ||
              g_calls[i].text[0] == '/');

    /* two windows: fonts 5 and 6; player 1 eleventh of 12 in its own place */
    gNumWindows = 2;
    placePos[1][0].x = 250, placePos[1][0].y = 10, placePos[1][1].x = 250, placePos[1][1].y = 120;
    hud_playerColor[1][0] = 50;
    pc_cars[0].place = 0, pc_cars[1].place = 10, g_ncalls = 0;
    animPlace(0);
    CHECK(g_calls[0].what == 'W' && g_calls[0].font == 5);
    CHECK(glyph(1, 242, 10, "1", 5, 1) && glyph(3, 250, 10, "/", 5, 0) && g_calls[5].what == 'W' &&
          g_calls[5].font == 6 && glyph(6, 258, 10, "1", 6, 0) && glyph(8, 264, 10, "2", 6, 0));
    /* player 1: "11/12" (its colour is 150) */
    CHECK(text_at(10, 235, 121, "1", 5, 0) && text_at(11, 234, 120, "1", 5, 150));
    CHECK(text_at(12, 243, 121, "1", 5, 0) && text_at(13, 242, 120, "1", 5, 150));
    CHECK(text_at(15, 250, 120, "/", 5, 1) && text_at(18, 258, 120, "1", 6, 1) && text_at(20, 264, 120, "2", 6, 1));
    CHECK(g_ncalls == 21);
    /* a finished player has none */
    pc_cars[1].finished = 1, g_ncalls = 0;
    animPlace(0);
    CHECK(count('T') == 8);
    pc_cars[1].finished = 0, gNumWindows = 1;
}

/* ---------------------------------------------------------------- the HUD order */

static void cars_in_race(int n, int humans)
{
    memset(pc_models, 0, sizeof pc_models);
    memset(pc_cars, 0, sizeof pc_cars);
    for (int i = 0; i < n; i++) {
        pc_models[i].slot = (int16_t)i;
        pc_models[i].drone_type = i < humans ? 2 : 1;
        pc_models[i].active = 1;
        pc_cars[i].place = (int8_t)(n - 1 - i);   /* the last slot leads */
    }
    for (int h = 0; h < humans; h++)
        gPlayers[h].slot = (uint8_t)h;
    gNumActiveCars = (int8_t)n, num_humans = (int16_t)humans, gNumCars = (int16_t)n;
    test_race_cars = n;
    pc_race_cars_begin();
}

static void test_order(void)
{
    /* 12 cars, one human: the human last, the 11 drones before it in place order (slot 11 leads, then 10...) */
    memset(dot_order_list, 0x55, sizeof dot_order_list);
    cars_in_race(12, 1);
    CHECK(hud_updateFrame(0) == 1 && markers_valid == 1);
    CHECK(dot_order_list[11] == 0);
    for (int e = 0; e < 11; e++)
        CHECK(dot_order_list[e] == 1 + e);   /* entry 10 the leader, slot 11 */
    CHECK(dot_order_list[12] == 0x55 && dot_order_list[15] == 0x55);   /* nothing past the 12 */

    /* 8 cars, two humans: entries 8-11 cleared, so their dots hide */
    memset(dot_order_list, 0x55, sizeof dot_order_list);
    cars_in_race(8, 2);
    hud_updateFrame(0);
    CHECK(dot_order_list[7] == 0 && dot_order_list[6] == 1 && dot_order_list[5] == 7 && dot_order_list[0] == 2);
    CHECK(dot_order_list[8] == -1 && dot_order_list[11] == -1 && dot_order_list[12] == 0x55);

    /* 6 cars: the N64's 6 entries cleared and filled, the rest as they were */
    memset(dot_order_list, 0x55, sizeof dot_order_list);
    cars_in_race(6, 1);
    hud_updateFrame(0);
    CHECK(dot_order_list[5] == 0 && dot_order_list[4] == 5 && dot_order_list[0] == 1);
    CHECK(dot_order_list[6] == 0x55 && dot_order_list[7] == 0x55 && dot_order_list[11] == 0x55);
}

/* ---------------------------------------------------------------- ShowHUD's race set */

static int32_t other_blit(Blit *b) { return 1; }
static int32_t dot(Blit *b) { return 1; }

/* A made-up race set: other entries, then for each of 4 viewers a frame and its 8 blips (AnimID viewer << 8 | car),
 * then 8 dots (AnimID 0-7) and one more entry. */
static void make_set(void)
{
    int n = 0;
    memset(D_80114C58, 0, sizeof D_80114C58);
    for (; n < 40; n++)
        D_80114C58[n].animfunc = (int (*)(void *))other_blit, D_80114C58[n].animid = (unsigned)n;
    for (int v = 0; v < 4; v++) {
        D_80114C58[n].animfunc = (int (*)(void *))other_blit, D_80114C58[n++].animid = (unsigned)v;   /* the frame */
        for (int k = 0; k < 8; k++, n++) {
            D_80114C58[n].texname = "BLIP";
            D_80114C58[n].animfunc = (int (*)(void *))animRadarBlip;
            D_80114C58[n].animid = (unsigned)(v << 8 | k);
            D_80114C58[n].width = (short)(10 + v), D_80114C58[n].zdepth = (unsigned)(20 + v);
        }
    }
    for (int k = 0; k < 8; k++, n++) {
        D_80114C58[n].texname = "DOT";
        D_80114C58[n].animfunc = (int (*)(void *))animDot;
        D_80114C58[n].animid = (unsigned)k;
        D_80114C58[n].height = 4;
    }
    D_80114C58[n].animfunc = (int (*)(void *))dot, D_80114C58[n++].animid = 7;   /* not a dot: no copies */
    CHECK(n == 85);
}

static void show_hud(int cars)
{
    test_race_cars = cars;
    pc_race_cars_begin();
    HudBlit1 = NULL, g_set = NULL, g_set_n = 0;
    ShowHUD();
}

/* An entry as NewMultiBlit reads it, field by field (a copy's padding is no part of it). */
static int same(const struct pc_multiblit *a, const struct pc_multiblit *b)
{
    return a->texname == b->texname && a->dulx == b->dulx && a->duly == b->duly && a->width == b->width &&
           a->height == b->height && a->top == b->top && a->bot == b->bot && a->left == b->left &&
           a->right == b->right && a->zdepth == b->zdepth && a->alpha == b->alpha && a->animfunc == b->animfunc &&
           a->animid == b->animid;
}

static void test_set(void)
{
    gGameState = 0, gRace = 0, expansion_pak_present = 0, gTrackNum = 0;
    make_set();
    /* up to 8 cars: the ROM's set itself */
    show_hud(6);
    CHECK(g_set == D_80114C58 && g_set_n == 89 && HudBlit2 == &g_voice);
    show_hud(8);
    CHECK(g_set == D_80114C58 && g_set_n == 89);
    /* 12 cars: each run of blips and the dots continued after its 7 with 8-11 */
    show_hud(12);
    CHECK(g_set != D_80114C58 && g_set_n == 89 + 5 * 4);
    int n = 0;
    for (int i = 0; i < 89; i++) {
        const struct pc_multiblit *e = &D_80114C58[i];
        CHECK(same(&g_set_copy[n++], e));
        if ((e->animfunc == (int (*)(void *))animRadarBlip || e->animfunc == (int (*)(void *))animDot) &&
            (e->animid & 0xF) == 7)
            for (unsigned k = 8; k < 12; k++, n++) {
                struct pc_multiblit want = *e;
                want.animid = (e->animid & ~0xFu) | k;
                CHECK(same(&g_set_copy[n], &want));
            }
    }
    CHECK(n == g_set_n);
    CHECK(g_set_copy[52].animid == 0x00B && g_set_copy[54].animid == 0x100);   /* viewer 0's 11, viewer 1's 0 */
    /* 10 cars: 8 and 9 */
    show_hud(10);
    CHECK(g_set_n == 89 + 5 * 2 && g_set_copy[49].animid == 0x008 && g_set_copy[50].animid == 0x009);
    /* stunt and battle make no race set */
    gRace = 6;
    show_hud(12);
    CHECK(g_set == NULL && g_set_n == 0);
    gRace = 0;
    test_race_cars = 6;
    pc_race_cars_begin();
}

/* ---------------------------------------------------------------- the dots and the blips of slots 8-11 */

static void test_dots(void)
{
    Blit b;
    /* 12 cars, one human (slot 0), last in the HUD order */
    cars_in_race(12, 1);
    memset(dot_order_list, 0x55, sizeof dot_order_list);
    hud_updateFrame(0);
    gNumWindows = 1, gShowTrackMap = 1, gMirrorWaves = 1;
    gPathMax.x = 1000, gPathMax.z = 1000, gPathMin.x = 0, gPathMin.z = 0, kMapSize = 64;
    for (int i = 0; i < 12; i++) {
        pc_cars[i].pos[0] = 100.0f + 50 * i, pc_cars[i].pos[2] = 500;
        pc_models[i].collidable = 1;
    }
    for (unsigned id = 8; id < 12; id++) {
        memset(&b, 0, sizeof b);
        b.AnimID = id, b.Height = 4, b.Width = 4, b.Hide = 1;
        g_ncalls = 0;
        CHECK(animDot(&b) == 1);
        CHECK(g_ncalls == 1 && g_calls[0].what == 'I' && g_calls[0].a == (id == 11 ? 0 : 7) && !b.Hide);
    }
    /* a blip for slot 11 on viewer 0's radar: a drone's icon */
    gShowRadar = 1, gGameState = 0, radarWidth = 64, radarHeight = 64;
    pc_models[11].active = 1, pc_models[11].drone_type = 1;
    pc_cars[0].pos[0] = 0, pc_cars[0].pos[2] = 0, pc_cars[11].pos[0] = 30, pc_cars[11].pos[2] = -100;
    memset(&b, 0, sizeof b);
    b.AnimID = 0x00B, b.Width = 64, b.Height = 8, b.AnimFunc = animRadarBlip, b.Hide = 1;
    g_ncalls = 0;
    CHECK(animRadarBlip(&b) == 1);
    CHECK(g_ncalls == 1 && g_calls[0].what == 'I' && g_calls[0].a == 5 && !b.Hide && b.AnimFunc);
    /* a slot not in the race: hidden for good */
    pc_models[10].active = 0;
    memset(&b, 0, sizeof b);
    b.AnimID = 0x00A, b.Width = 64, b.Height = 8, b.AnimFunc = animRadarBlip;
    g_ncalls = 0;
    CHECK(animRadarBlip(&b) == 1 && g_ncalls == 0 && b.Hide && !b.AnimFunc);
}

/* ---------------------------------------------------------------- the results' finishing place */

static char g_profile[64];
static char *g_profile_p = g_profile;
static char g_bank[12][8];

/* The bank: the six ordinals made of the number and `suffix`, then six entries of something else. */
static void bank(const char *suffix, const char *after)
{
    for (int i = 0; i < 6; i++) {
        snprintf(g_bank[i], sizeof g_bank[i], "%d%s", i + 1, suffix);
        snprintf(g_bank[6 + i], sizeof g_bank[6 + i], "%s", after);
        g_strings[g_first + i] = g_bank[i], g_strings[g_first + 6 + i] = g_bank[6 + i];
    }
}

/* The finishing place drawn in the one player's row (the fourth column's middle). */
static const char *place_text(int place)
{
    pc_cars[0].place = (int8_t)place, g_ncalls = 0;
    drawRaceStandings();
    for (int i = 0; i < g_ncalls; i++)
        if (g_calls[i].what == 'T' && g_calls[i].a == 250 && g_calls[i].b == 57)
            return g_calls[i].text;
    return "(none)";
}

static void test_results(void)
{
    static char empty[] = "-";
    for (int i = 0; i < 300; i++)
        g_fixed[i] = empty;
    g_indices[54] = g_first;   /* TextIdx2's cars, +108 */
    memset(pc_models, 0, sizeof pc_models);
    memset(pc_cars, 0, sizeof pc_cars);
    gNumPlayers = 1, gNumCars = 12, gRace = 0, gRankedPlayer[0] = 0;
    strcpy(g_profile + PROFILE_OFS(20), "ME");
    gPlayers[0].name = &g_profile_p;
    gLap[0] = 1, gNumLaps = 3;

    bank("zq", "q");   /* a suffix, and after the six something else */
    CHECK(!strcmp(place_text(0), "1zq") && !strcmp(place_text(5), "6zq"));   /* the six from the bank */
    CHECK(!strcmp(place_text(6), "7zq") && !strcmp(place_text(9), "10zq") && !strcmp(place_text(11), "12zq"));
    bank(".", "7.");   /* a full stop; the entries after the six are ordinals again */
    CHECK(!strcmp(place_text(6), "7.") && !strcmp(place_text(11), "12."));
    bank("", "");      /* the number alone; empty entries after the six */
    CHECK(!strcmp(place_text(7), "8") && !strcmp(place_text(10), "11") && !strcmp(place_text(2), "3"));
    bank("zq", "q");   /* up to the sixth, the bank's own string */
    place_text(3);
    for (int i = 0; i < g_ncalls; i++)
        if (g_calls[i].what == 'T' && g_calls[i].a == 250 && g_calls[i].b == 57)
            CHECK(!strcmp(g_calls[i].text, g_strings[g_first + 3]));
}

int main(void)
{
    test_place();
    test_order();
    test_set();
    test_dots();
    test_results();
    return TEST_DONE();
}
