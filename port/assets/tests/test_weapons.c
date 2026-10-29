/* port/assets/tests/test_weapons.c: the weapon icons, the targets that the target info table (D_80117530, 0x40 bytes
 * a record on the PC) names the start and visual functions of: StartWeaponIcon (_StartWeaponIcon) and func_8010D680
 * (_AnimateWeaponIcon), from their drafts in nonmatching/game (docs/port/POINTERS.md 10). The icon is a level-object
 * node (0x70 bytes, its N64 layout) in the RDRAM window, the cars have their PC layout (0x468 bytes, docs/port/CARS.md)
 * and the model records their N64 one. Cars, records and nodes past the first are used, so an N64 stride or a field
 * at its N64 offset lands somewhere else. The window is mapped by the test (exit 77, skipped, if it can't).
 * game: nonmatching/game/StartWeaponIcon.c nonmatching/game/func_8010D680.c
 */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the PC layouts */

typedef struct {
    const char *name, *file;
    void (*start)(void *);
    void (*visual)(void *, short);
    int16_t kind;
    uint16_t flags;
    int16_t shape;
    int8_t category, element;
    float value;
    int32_t sound;                   /* N64 +0x1C */
    int32_t w20, w24, w28;
    float f2C;
} TInfo;
_Static_assert(sizeof(TInfo) == TINFO_OFS(0x30) && offsetof(TInfo, sound) == TINFO_OFS(0x1C), "the record");

#define NF(n, off, T) (*(T *)((uint8_t *)(n) + (off)))   /* a field at its N64 offset (node, model) */
#define CF(c, off, T) (*(T *)((uint8_t *)(c) + CAR_OFS(off)))   /* a car field by its N64 offset */
#define W32(p) ((uint32_t)(uintptr_t)(p))
#define WP(a) ((void *)(uintptr_t)(a))

/* ---------------------------------------------------------------- the game's data */

#define NODE(i) ((uint8_t *)WP(0x8014D280u + 0x70u * (i)))   /* the node block D_8014D280 (game arena) */
#define HEAP 0x80300000u
TInfo D_80117530[122];
__attribute__((aligned(16))) uint8_t D_80152818[6 * PC_CAR_SIZE];
#define CAR(i) (D_80152818 + PC_CAR_SIZE * (i))
struct pc_model pc_models[PC_MODEL_SLOTS];    /* the model records (pc_game_types.h) */
#define MODEL(i) ((uint8_t *)&pc_models[i])
int32_t D_80121D60[8] = {10, 11, 12, 13, 14, 15, 16, 17};
float D_8011F844[4][4] = {{0}, {2.0f, 4.0f, 3.0f, 0.0f}, {0}, {0}};   /* colfront, colrear, colside, colheight */
uint8_t D_8012E700[600 * 0x68];      /* the scene nodes, host layout: the flags first */
int32_t D_801170FC;
volatile float D_8002EB94 = 0.5f;
int16_t D_8014A108;

/* ---------------------------------------------------------------- what the code under test calls */

static int32_t g_sound, g_sound_slot;
static void *g_sound_pos;
static uint8_t g_sound_mode;
int32_t stat_lap_split(int32_t sound, int32_t slot, float *pos, uint8_t mode)
{
    g_sound = sound, g_sound_slot = slot, g_sound_pos = pos, g_sound_mode = mode;
    return 0;
}
static int32_t g_hidden, g_shown;
void model_data_load(int32_t obj, int32_t mode, uint32_t v) { g_hidden = obj; }
void model_transform_setup(int32_t obj, int32_t mode, uint32_t v) { g_shown = obj; }
static int g_powerups;
void func_80391490(void) { g_powerups++; }
static void *g_released;
static int32_t g_unlink;
void entity_transform_apply(void *v, int32_t unlink) { g_released = v, g_unlink = unlink; }
static float g_rate[3];
static void *g_uvs;
void sound_position_set(float *rate, float (*uvs)[3]) { memcpy(g_rate, rate, sizeof g_rate), g_uvs = uvs; }

void StartWeaponIcon(void *t);
void func_8010D680(struct pc_visual *v, int16_t op);

/* ---------------------------------------------------------------- tests */

#define INFO 7                       /* the icon's info record: past the first */

static uint8_t *icon(int type, uint8_t flags, int8_t slot)
{
    uint8_t *t = NODE(5);
    memset(t, 0, 0x70);
    NF(t, 4, uint8_t) = flags;
    NF(t, 0x0C, int32_t) = 0x10021;  /* the scene-node handle: its low half is the node, 33 */
    NF(t, 0x10, int16_t) = INFO;
    NF(t, 0x38, float) = 1.0f, NF(t, 0x3C, float) = 2.0f, NF(t, 0x40, float) = 3.0f;
    NF(t, 0x50, int16_t) = (int16_t)type;
    NF(t, 0x5C, int8_t) = slot;
    g_sound = 0, g_hidden = 0, g_shown = 0, g_powerups = 0;
    return t;
}

/* Set-up (flag 4 clear): one more power-up for battle mode, the icon hidden. */
static void test_setup(void)
{
    uint8_t *t = icon(351, 0x02 | 0x08, 0);
    StartWeaponIcon(t);
    CHECK(g_powerups == 1 && NF(t, 4, uint8_t) == (0x08 | 0x20) && g_hidden == 0x10021 && g_sound == 0);
}

/* A pickup (flag 4, the car in the slot): the info record's sound at the icon, and the car's weapon. */
static void test_pickup(void)
{
    uint8_t *t, *car = CAR(3);

    memset(car, 0, PC_CAR_SIZE);
    CF(car, 0x384, int8_t) = 1, CF(car, 0x385, int8_t) = 4;   /* weapon 1, 4 shots */
    t = icon(351, 0x04 | 0x02 | 0x08, 3);
    StartWeaponIcon(t);
    CHECK(g_sound == 0x5151 && g_sound_slot == 3 && g_sound_pos == t + 0x38 && g_sound_mode == 2);
    CHECK(NF(t, 4, uint8_t) == 0x08 && g_powerups == 0);
    CHECK(CF(car, 0x384, int8_t) == 1 && CF(car, 0x385, int8_t) == 4 + 11);   /* the same weapon: more shots */

    t = icon(356, 0x04, 3);                                     /* another weapon replaces it */
    StartWeaponIcon(t);
    CHECK(CF(car, 0x384, int8_t) == 4 && CF(car, 0x385, int8_t) == 14);
    t = icon(349, 0x04, 3);                                     /* an unknown kind: weapon 1 */
    StartWeaponIcon(t);
    CHECK(CF(car, 0x384, int8_t) == 1 && CF(car, 0x385, int8_t) == 11);

    t = icon(353, 0x04, 3);                                     /* health */
    StartWeaponIcon(t);
    CHECK(CF(car, 0x386, int16_t) == 800 && CF(car, 0x384, int8_t) == 1);

    t = icon(354, 0x04, 3);                                     /* invisibility */
    StartWeaponIcon(t);
    CHECK(CF(car, 0x38C, uint32_t) == 1 && CF(car, 0x390, float) == 30.0f && CF(car, 0x3A2, int8_t) == 1);
    CHECK(CF(car, 0x3A8, float) == 0.666667f && CF(car, 0x3A1, uint8_t) == 255);

    t = icon(359, 0x04, 3);                                     /* shield */
    StartWeaponIcon(t);
    CHECK(CF(car, 0x38C, uint32_t) == (1 | 0xA) && CF(car, 0x394, float) == 0.0333333f);
    CHECK(CF(car, 0x398, float) == 0.05f && CF(car, 0x39C, float) == 0.0f);

    /* weapon 5 grows the car's collision body from its kind's size (D_8011F844[body type]) */
    memset(MODEL(3), 0, 0x808);
    NF(MODEL(3), 8, uint8_t) = 1;
    t = icon(357, 0x04, 3);
    StartWeaponIcon(t);
    CHECK(CF(car, 0x384, int8_t) == 5 && CF(car, 0x385, int8_t) == 15);
    CHECK(NF(MODEL(3), 0xFC, float) == 5.0f && NF(MODEL(3), 0x108, float) == 5.0f);
    CHECK(fabsf(NF(MODEL(3), 0x654, float) - sqrtf(34.0f)) < 1e-5f && NF(MODEL(2), 0x654, float) == 0.0f);

    /* a car with appearance bit 0x10 takes nothing: the icon is flagged and nothing sounds */
    CF(car, 0xE8, uint32_t) = 0x10;
    t = icon(352, 0x04, 3);
    StartWeaponIcon(t);
    CHECK(NF(t, 4, uint8_t) == 0x02 && g_sound == 0 && CF(car, 0x384, int8_t) == 5);
    CF(car, 0xE8, uint32_t) = 0;
}

/* The visual: the icon through the visual's 32-bit data. */
static void test_visual(void)
{
    struct pc_visual v;
    uint8_t *t, *rate = WP(HEAP + 0x100u);

    memset(&v, 0, sizeof v);
    t = icon(352, 0x02, 0);
    v.data = W32(t);
    func_8010D680(&v, 0);                                      /* released */
    CHECK(g_released == &v && g_unlink == 1);

    NF(rate, 0, float) = 2.0f, NF(rate, 4, float) = 4.0f, NF(rate, 8, float) = 6.0f;
    NF(t, 0x6C, uint32_t) = W32(rate);                         /* its state: the spin rates, a heap record */
    D_801170FC = 1, g_uvs = NULL;
    func_8010D680(&v, 1);                                      /* paused */
    CHECK(g_uvs == NULL);
    D_801170FC = 0;
    func_8010D680(&v, 1);                                      /* shown (flag 2): it spins */
    CHECK(g_uvs == t + 0x14 && g_rate[0] == 1.0f && g_rate[1] == 2.0f && g_rate[2] == 3.0f);

    /* hidden: shown again once no car has its weapon (the scene node's flag 31 says it is ready) */
    NF(t, 4, uint8_t) = 0, g_hidden = 0;
    func_8010D680(&v, 1);
    CHECK(g_hidden == 0x10021 && g_shown == 0);
    NF(D_8012E700 + 33 * 0x68, 0, uint32_t) = 0x80000000u;
    D_8014A108 = 3;
    memset(CAR(2), 0, PC_CAR_SIZE);
    CF(CAR(2), 0x384, int8_t) = 2;                             /* car 2 has weapon 2 */
    g_hidden = 0;
    func_8010D680(&v, 1);
    CHECK(g_shown == 0 && g_hidden == 0 && NF(t, 4, uint8_t) == 0);
    CF(CAR(2), 0x384, int8_t) = 0;
    func_8010D680(&v, 1);
    CHECK(g_shown == 0x10021 && NF(t, 4, uint8_t) == 0x02);
    NF(t, 4, uint8_t) = 0, NF(t, 0x50, int16_t) = 353, g_shown = 0;   /* not a weapon: stays */
    func_8010D680(&v, 1);
    CHECK(g_shown == 0 && g_hidden == 0 && NF(t, 4, uint8_t) == 0);
}

int main(void)
{
    if (!test_rdram()) {
        fprintf(stderr, "test_weapons: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    for (int i = 0; i < 122; i++)
        D_80117530[i].sound = -1;
    D_80117530[INFO].sound = 0x5151;
    test_setup();
    test_pickup();
    test_visual();
    return TEST_DONE();
}
