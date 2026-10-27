/* port/assets/tests/test_carscr.c: runtime image A's func_803A44FC (animText@carscr.c, with animButtonText inlined),
 * the car-select screen's per-player text, from its draft in nonmatching/ovl_a. It reaches the records the front end
 * builds in their PC layouts: the input records (80 bytes, the profile handle at +0x48), the profiles and the ghost
 * records through their handles (pointer fields host-sized), the text bank's three string tables (ModelTables: +8,
 * +0x18, +0x20) and the car-select slots (64 bytes, N64 layout). Player 1 and records past the first are used, so an
 * N64 stride or offset lands somewhere else. The test stands in for the text and projection functions and records
 * what is drawn where.
 * game: nonmatching/ovl_a/func_803A44FC.c
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the PC layouts */

typedef struct Profile { void *next, *previous, *file; uint32_t id0, id1; char name[24]; void *stats; } Profile;
typedef struct Player {
    uint32_t held, pressed, w8, repeated; uint8_t w10[50]; uint8_t paint, w43[5]; Profile **owner;
} Player;
typedef struct Record {
    void *next, *file; int8_t track; uint8_t mode; char name[14]; uint32_t id0, id1; float time; void *w24, *active;
} Record;
typedef struct Slot { int32_t kind, handle; float angle, matrix[9], position[3]; uint8_t alpha, pad[3]; } Slot;
_Static_assert(sizeof(Player) == 0x50 && offsetof(Player, owner) == 0x48, "the input record");
_Static_assert(offsetof(Profile, name) == 0x20 && offsetof(Record, time) == 0x28 && sizeof(Slot) == 0x40, "records");

/* ---------------------------------------------------------------- the game's data */

static char *g_single[300], *g_indexed[100];
static uint16_t g_indices[16];
struct {
    uint16_t count0; char **ptrs0; uint16_t count1, count2; uint16_t *data1; char **ptrs2;
    uint16_t count3, count4, count5; uint16_t *data3, *data4; char **ptrs5;
} countdown_state = {.count0 = 300, .ptrs0 = g_single, .count1 = 16, .data1 = g_indices, .ptrs2 = g_indexed};
int16_t D_8014A108, D_80151AD0, D_803B28B0[4];
int32_t D_8014A110, D_803B9B90[4], D_803BA050[4], D_803BA060[4][19], D_803AF980, D_801146AC[4];
Player D_8014A118[4];
uint8_t D_80150B70[4 * 152];                   /* Camera[]: 152 bytes */
Slot D_803AF9A8[4][44];
float D_803B25AC = 2.0f, D_803B25C0 = 300.0f;
int8_t D_803BA028[4], D_803BA00C, D_803B9FD0[4], gCarTeam[4];
Record **D_80152698[4];
uint32_t D_801146BC, D_801146C0;
char *D_80110D70[8];
int8_t D_80110E78[5][13], D_8011103C[5][13], D_80111080[5][13], D_8011119C[5][13], D_80111230[5][13];
int8_t D_8011128C[5][13], D_8011157C[5][13], D_801115C0[5][13];

/* ---------------------------------------------------------------- what the code under test calls */

typedef struct { char what; int32_t a, b, c, color; char text[24]; } Call;
static Call g_calls[96];
static int g_ncalls, g_color = -1;
static Call *record(char what)
{
    Call *c = &g_calls[g_ncalls < 96 ? g_ncalls++ : 95];
    memset(c, 0, sizeof *c);
    c->what = what, c->color = g_color;
    return c;
}
void render_helper(float depth) { record('R')->a = (int32_t)depth; }
int32_t object_create(int32_t font) { record('F')->a = font; return 0; }
void dispatch_handler(int32_t color) { g_color = color; }
void func_800B669C(int32_t h, int32_t v) { record('J')->a = h * 10 + v; }
static void *g_views[16];
static int g_nviews;
/* The projection: screen x, y from the position's x, y, plus the player's index x 1000 on x. */
void brake_light_update(int32_t idx, float *pos, void *view, float *vout, int16_t *sout)
{
    g_views[g_nviews++ & 15] = view;
    sout[0] = (int16_t)(pos[0] + 1000 * idx), sout[1] = (int16_t)pos[1];
}
int16_t sound_pitch_diff_halved(void *s, int16_t x) { return (int16_t)(x - (int16_t)strlen(s)); }   /* centred */
int16_t object_bytes_sum_global(void) { return 9; }
void state_utility(int16_t x, int16_t y, void *text)
{
    Call *c = record('T');
    c->a = x, c->b = y;
    strncpy(c->text, text, sizeof c->text - 1);
}
void fcvt_wrapper(char *out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsprintf(out, fmt, ap);
    va_end(ap);
}
void music_fade(int16_t x, int16_t y, int16_t w, void *text)
{
    Call *c = record('E');
    c->a = x, c->b = y, c->c = w;
    strncpy(c->text, text, sizeof c->text - 1);
}
void func_800ED66C(float alpha) { record('A')->a = (int32_t)alpha; }
void func_800F6928(float depth) { record('Z')->a = (int32_t)depth; }
static uint32_t g_flash[2];
void func_800BEA3C(uint32_t first, uint32_t second) { g_flash[0] = first, g_flash[1] = second; }
uint8_t *func_800BE6A4(uint8_t *dst, uint8_t *src) { return (uint8_t *)strcpy((char *)dst, (char *)src); }

int32_t func_803A44FC(void *blt);

/* ---------------------------------------------------------------- tests */

static int find_text(const char *s)
{
    for (int i = 0; i < g_ncalls; i++)
        if (g_calls[i].what == 'T' && !strcmp(g_calls[i].text, s))
            return i;
    return -1;
}

static Profile g_profiles[4];
static Profile *g_profile_ptrs[4];
static Record g_ghost, *g_ghost_ptr = &g_ghost;

static void setup(void)
{
    static const char *names[4] = {"ANNA", "BOB", "CARL", "DORA"};
    for (int i = 0; i < 4; i++) {
        strcpy(g_profiles[i].name, names[i]);
        g_profile_ptrs[i] = &g_profiles[i];
        D_8014A118[i].owner = &g_profile_ptrs[i];
        D_801146AC[i] = 30 + i;
    }
    g_single[31] = "PLAYERS", g_single[151] = "WAITING FOR", g_single[224] = "NO GHOST", g_single[225] = "NO TIMES";
    for (int i = 0; i < 100; i++) {
        static char buf[100][8];
        sprintf(buf[i], "S%d", i);
        g_indexed[i] = buf[i];
    }
    g_indices[2] = 0, g_indices[3] = 40, g_indices[15] = 90;
    D_80110D70[3] = "RUSHER";
    D_803B9FD0[1] = 3;
    D_803B28B0[1] = 4;
    strcpy(g_ghost.name, "GHOSTY");
    g_ghost.time = 83.25f;                       /* 1:23.250 */
    D_801146BC = 0x11223344, D_801146C0 = 0x55667788;
    /* player 1's slots: the button at 38, options 0, 15 and 16 with their values at +19 */
    D_803AF9A8[1][38].position[0] = 10, D_803AF9A8[1][38].position[1] = 20;
    D_803AF9A8[0][38].position[0] = 5, D_803AF9A8[0][38].position[1] = 60;
    D_803AF9A8[1][0].angle = 0.1f, D_803AF9A8[1][0].alpha = 200, D_803AF9A8[1][0].position[2] = 50.0f;
    D_803AF9A8[1][0].position[0] = 100, D_803AF9A8[1][0].position[1] = 70;
    D_803AF9A8[1][19].position[0] = 200, D_803AF9A8[1][19].position[1] = 70;
    D_803AF9A8[1][15].angle = 2.0f, D_803AF9A8[1][15].alpha = 200;      /* turned away: skipped */
    D_803AF9A8[1][16].angle = 3.0f, D_803AF9A8[1][16].alpha = 100, D_803AF9A8[1][16].position[2] = 40.0f;
    D_803AF9A8[1][16].position[0] = 120, D_803AF9A8[1][16].position[1] = 90;
    D_803AF9A8[1][35].position[0] = 220, D_803AF9A8[1][35].position[1] = 90;
    D_803BA050[1] = 3;
    D_803BA060[1][0] = 0, D_803BA060[1][1] = 15, D_803BA060[1][2] = 16;
}

/* Player 1 choosing, in a ghost race with one ghost: its name, the ghost's name and time, and its options. */
static void test_choosing(void)
{
    int i;

    D_8014A108 = 2, D_80151AD0 = 2, D_8014A110 = 2, D_803BA00C = 1;
    D_803B9B90[0] = 0, D_803B9B90[1] = 1;        /* player 0's buttons aren't up: nothing */
    D_803BA028[0] = D_803BA028[1] = 0;
    D_80152698[1] = &g_ghost_ptr;
    g_ncalls = 0, g_nviews = 0;
    CHECK(func_803A44FC(NULL) == 1);
    CHECK(g_views[0] == D_80150B70 + 152 * 1);   /* player 1's camera */
    CHECK(g_calls[0].what == 'R' && g_calls[1].what == 'J' && g_calls[1].a == 11);
    CHECK(g_calls[2].what == 'F' && g_calls[2].a == 12);                    /* two windows */
    i = 3;
    CHECK(g_calls[i].what == 'E' && g_calls[i].a == 1010 && g_calls[i].b == 20 && g_calls[i].c == 69);
    CHECK(!strcmp(g_calls[i].text, "BOB") && g_calls[i].color == 1);       /* through the profile handle */
    i = find_text("GHOST 1");
    CHECK(i >= 0 && g_calls[i].a == 1005 - 7 && g_calls[i].b == 60 && g_calls[i].color == 1);
    i = find_text("GHOSTY");                     /* the ghost's name below it, in the ghost's colour */
    CHECK(i >= 0 && g_calls[i].a == 1005 - 6 && g_calls[i].b == -30 && g_calls[i].color == 31);
    i = find_text("TIME 1:23.250");
    CHECK(i >= 0 && g_calls[i].b == -30 + 9 && g_calls[i].color == 1);
    CHECK(g_flash[0] == 0x11223344 && g_flash[1] == 0x55667788);
    /* option 0: its label (text table 3 for two windows) and the car's title; option 16: ghost 1's name */
    i = find_text("S40");
    CHECK(i >= 0 && g_calls[i].a == 1100 - 45 && g_calls[i].b == 70 && g_calls[i].color == 1);
    CHECK(g_calls[i - 2].what == 'A' && g_calls[i - 2].a == 200);   /* its alpha, then its depth */
    CHECK(g_calls[i - 1].what == 'Z' && g_calls[i - 1].a == 40);
    i = find_text("RUSHER");
    CHECK(i >= 0 && g_calls[i].a == 1200 - 50 && g_calls[i].b == 70);
    CHECK(find_text("S55") < 0);                 /* option 15 is turned away */
    i = find_text("S56");
    CHECK(i >= 0 && g_calls[i].a == 1120 - 45 && g_calls[i].color == 22);   /* past a quarter turn */
    i = find_text("S56") + 1;
    CHECK(g_calls[i].what == 'T' && !strcmp(g_calls[i].text, "GHOSTY") && g_calls[i].a == 1220 - 50);
    CHECK(g_calls[g_ncalls - 1].what == 'R' && g_calls[g_ncalls - 2].what == 'A' && g_calls[g_ncalls - 2].a == -1);

    /* no ghost record: text 224 or 225; option 16 without ghost 1: text table 15 */
    D_80152698[1] = NULL, D_803AF980 = 0, g_ncalls = 0;
    CHECK(func_803A44FC(NULL) == 1);
    CHECK(find_text("NO TIMES") >= 0 && find_text("GHOSTY") < 0 && find_text("S90") >= 0);
}

/* Player 1 ready, player 0 not: the message and who is being waited for, then nothing else. */
static void test_ready(void)
{
    int i;

    D_8014A110 = 0, D_803BA028[1] = 1, D_803BA028[0] = 0, g_ncalls = 0;
    CHECK(func_803A44FC(NULL) == 1);
    i = find_text("WAITING FOR");
    CHECK(i >= 0 && g_calls[i].a == 1000 - 11 && g_calls[i].b == -120 - 9 && g_calls[i].color == 1);
    CHECK(g_calls[i + 1].what == 'T' && !strcmp(g_calls[i + 1].text, "ANNA") && g_calls[i + 1].b == -120);
    CHECK(g_calls[i + 1].color == 30 && find_text("S40") < 0 && g_calls[g_ncalls - 1].what == 'A');

    D_8014A108 = 3, D_803BA028[2] = 0, g_ncalls = 0;   /* two waited for */
    CHECK(func_803A44FC(NULL) == 1);
    CHECK(find_text("PLAYERS") >= 0 && find_text("ANNA") < 0);
    D_803BA028[0] = D_803BA028[2] = 1, g_ncalls = 0;   /* nobody */
    CHECK(func_803A44FC(NULL) == 1);
    i = find_text("GET READY TO RACE");
    CHECK(i >= 0 && g_calls[i].b == -120 && find_text("WAITING FOR") < 0);
}

int main(void)
{
    setup();
    test_choosing();
    test_ready();
    return TEST_DONE();
}
