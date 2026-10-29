/* port/assets/tests/test_hudblit.c: blit animation functions that m2c wrote at the Blit's N64 offsets, on its host
 * layout (docs/port/POINTERS.md 9.3, docs/port/BRINGUP.md 15). animBones (func_80105DA8), the crossbones by the
 * speedometer, which only deaths mode shows: outside it, the N64 offsets set the low byte of X (host +0x1A) to 1
 * instead of Hide (host +0x26), so every race drew the skull at the top left; deaths mode places it at
 * bonesPos[windows - 1][player] less half its width, and hides it for a player past the windows or whose car is
 * locked in place. callback_init (the idle overlay) and struct_callback_init (the settings dialog) make a colour
 * their Image (host +8; the N64's +4 split the pointer over the Name and the Image), and the second clears its
 * AnimFunc (host +0x38); sound_volume_helper shows its blit on the results screen. The test stands in for
 * UpdateBlit and records the blit it is given. The values are made up.
 * game: src/blob/func_80105DA8.c src/blob/callback_init.c src/blob/struct_callback_init.c
 * game: src/blob/sound_volume_helper.c
 */
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* sound_control.c's Blit: the layout every PC view has. */
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
_Static_assert(sizeof(Blit) == 0x68, "the Blit's host size");
_Static_assert(offsetof(Blit, Hide) == 0x26 && offsetof(Blit, AnimID) == 0x40, "the Blit's host offsets");

/* ---------------------------------------------------------------- the game's data */

int8_t D_80142760;                                     /* gDeathMode */
int16_t gNumWindows;
struct pc_model pc_models[PC_MODEL_SLOTS];             /* the model records (pc_game_types.h): model[i].slot */
uint8_t D_80152907[6 * PC_CAR_SIZE];                   /* car[slot] +0xEF, locked in place, the PC stride */
struct { int32_t x, y; } D_801160C8[4][4];             /* bonesPos [windows - 1][player] */
uint8_t D_801146FC[4], D_80116DE0[4];                  /* the idle overlay's and the dialog's colours */
int32_t state_word_a;                                  /* gGameState */
#define MODEL_SLOT(i) (pc_models[i].slot)
#define CAR_LOCKED(s) (*(int8_t *)(D_80152907 + PC_CAR_SIZE * (s)))

/* ---------------------------------------------------------------- what the code under test calls */

static int g_updates;
static Blit g_seen;
void Input_ApplyPadConfig(Blit *b) { g_updates++, g_seen = *b; }   /* UpdateBlit */

int32_t func_80105DA8(Blit *b);   /* animBones */
int32_t callback_init(Blit *b);
int32_t struct_callback_init(Blit *b);
int32_t sound_volume_helper(Blit *b);

/* ---------------------------------------------------------------- tests */

static const char g_name[] = "BONES";

static void blit(Blit *b, uint32_t player)
{
    memset(b, 0, sizeof *b);
    b->Name = g_name;
    b->X = 100, b->Y = 50, b->Width = 26, b->Height = 13, b->Alpha = 254, b->Init = 1;
    b->AnimFunc = func_80105DA8, b->AnimID = player;
    g_updates = 0;
}

/* Not deaths mode: hidden, once, and nothing else changed. */
static void test_off(void)
{
    Blit b;

    D_80142760 = 0, gNumWindows = 1;
    blit(&b, 0);
    CHECK(func_80105DA8(&b) == 1);
    CHECK(b.Hide == 1 && g_updates == 1 && g_seen.Hide == 1);
    CHECK(b.X == 100 && b.Y == 50 && b.Width == 26 && b.Left == 0 && b.Right == 0 && b.AnimID == 0);
    CHECK(b.AnimFunc == func_80105DA8 && b.Name == g_name);
    CHECK(func_80105DA8(&b) == 1 && g_updates == 1);   /* already hidden: no update */
}

/* Deaths mode: each window's player at its place, less half the skull's width. */
static void test_on(void)
{
    Blit b;

    D_80142760 = 1;
    D_801160C8[0][0].x = 210, D_801160C8[0][0].y = 6;
    D_801160C8[1][0].x = 140, D_801160C8[1][0].y = 7;
    D_801160C8[1][1].x = 160, D_801160C8[1][1].y = 118;
    MODEL_SLOT(0) = 4, MODEL_SLOT(1) = 2;
    CAR_LOCKED(4) = 0, CAR_LOCKED(2) = 0;

    gNumWindows = 1;
    blit(&b, 0);
    CHECK(func_80105DA8(&b) == 1);
    CHECK(b.Hide == 0 && b.X == 197 && b.Y == 6 && g_updates == 1 && g_seen.X == 197 && g_seen.Y == 6);

    gNumWindows = 2;
    blit(&b, 1);
    b.Width = 27;   /* 27 / 2 = 13 */
    CHECK(func_80105DA8(&b) == 1);
    CHECK(b.Hide == 0 && b.X == 147 && b.Y == 118 && g_updates == 1);
    CHECK(b.Image == NULL && b.Info == NULL && b.AnimID == 1);   /* the pointers before X stay as they were */
}

/* Deaths mode, hidden: a player past the windows, and a player whose car is locked in place. */
static void test_hidden(void)
{
    Blit b;

    D_80142760 = 1, gNumWindows = 2;
    blit(&b, 2);
    CHECK(func_80105DA8(&b) == 1 && b.Hide == 1 && b.X == 100 && g_updates == 1);
    blit(&b, 3);
    CHECK(func_80105DA8(&b) == 1 && b.Hide == 1);

    MODEL_SLOT(0) = 5, CAR_LOCKED(5) = 1;
    blit(&b, 0);
    CHECK(func_80105DA8(&b) == 1 && b.Hide == 1 && b.X == 100 && b.Y == 50 && g_updates == 1);
    CAR_LOCKED(5) = 0;
    blit(&b, 0);
    D_801160C8[1][0].x = 40;
    CHECK(func_80105DA8(&b) == 1 && b.Hide == 0 && b.X == 27 && b.Y == 7);
}

static int32_t keep(Blit *b) { return 1; }
static uint8_t g_white[4] = {255, 255, 255, 255};

/* The idle overlay and the settings dialog: the colour becomes the Image, the rest stays. */
static void test_colours(void)
{
    Blit b;

    blit(&b, 0);
    b.Image = g_white, b.Info = (void *)keep, b.Top = 3, b.Bot = 9;
    CHECK(callback_init(&b) == 1);
    CHECK(b.Image == D_801146FC && g_updates == 1 && g_seen.Image == D_801146FC);
    CHECK(b.Name == g_name && b.Info == (void *)keep && b.AnimFunc == func_80105DA8);

    blit(&b, 0);
    b.Image = g_white, b.AnimFunc = keep, b.Top = 3, b.Bot = 9;
    CHECK(struct_callback_init(&b) == 1);
    CHECK(b.Image == D_80116DE0 && g_updates == 1 && g_seen.Image == D_80116DE0 && g_seen.AnimFunc == keep);
    CHECK(b.AnimFunc == NULL && b.Top == 3 && b.Bot == 9 && b.Name == g_name);
}

/* Shown on the results screen (gGameState bit 25), once. */
static void test_results(void)
{
    Blit b;

    blit(&b, 0);
    b.Hide = 1, state_word_a = 0x400000;
    CHECK(sound_volume_helper(&b) == 1 && b.Hide == 1 && g_updates == 0);
    state_word_a = 0x2000000;
    CHECK(sound_volume_helper(&b) == 1 && b.Hide == 0 && b.X == 100 && g_updates == 1 && g_seen.Hide == 0);
    CHECK(sound_volume_helper(&b) == 1 && g_updates == 1);
}

int main(void)
{
    test_off();
    test_on();
    test_hidden();
    test_colours();
    test_results();
    return TEST_DONE();
}
