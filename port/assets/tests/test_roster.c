/* port/assets/tests/test_roster.c: the race roster (gLink: pc_links, struct pc_link, port/include/pc_game_types.h)
 * through the game's own readers and writers, with more slots declared than the game walks (docs/port/DECOUPLE.md).
 * This test and its game objects are built with PC_MAX_LINKS 16 (CMakeLists.txt), so the storage has 16 slots
 * where the game build has 13. Every slot starts as a sentinel: the code must write and read the fields at their
 * N64 offsets in slots it walks (13 at most, 6 for a race) and leave the others alone, whatever the storage's size.
 * func_8039156C names the 13 slots' cars; func_800EC190 resets the race's 6; CalcPolePositions (init_state_continue)
 * fills them for a race; func_800EC914 (munge_gLink_data) counts the active ones into the race's car count
 * D_80152744 and builds the human and drone lists; stat_lap_split reads a slot's owner through what was the alias
 * D_80153E8F, +7 with a stride of 8.
 * game: src/ovl_a/func_8039156C.c src/blob/func_800EC190.c src/blob/groups/update_game/init_state_continue.c
 * game: src/blob/func_800EC914.c src/blob/stat_lap_split.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_link pc_links[PC_LINK_SLOTS];      /* data.c defines it in the game */
#define RAW(i, off) (((const unsigned char *)pc_links)[(i) * 8 + (off)])
#define SENTINEL 0xEE                        /* flags 0xEE has LINK_ACTIVE (0x80) set */

int8_t D_8014978C, D_80154628, D_80152744, D_80150C04, D_80150F14, D_8010FFC0;
volatile int16_t D_801543CA, D_80152768, D_80153FD2;
int16_t D_8014A108, D_80142724, D_8015274C, D_80143A40[8], D_801527D8[8], D_80152808[8];
int16_t D_8011F020[16], D_8011F040[16];
int32_t D_8014A110, D_80143FF4;
uint8_t D_801543D4, D_8014A118[6 * 80], D_80154450[6 * 0x4C], D_803BA230[16 * 12] __attribute__((aligned(4)));
struct pc_model pc_models[PC_MODEL_SLOTS];    /* the model records (pc_game_types.h), N64 layout */
uint8_t D_80152818[6 * PC_CAR_SIZE] __attribute__((aligned(16)));   /* the car records, PC layout */
ALIAS(player_array, D_80152818, 0);

#define MODEL16(i, off) (*(int16_t *)((uint8_t *)&pc_models[i] + (off)))
#define MODEL8(i, off) (*(int8_t *)((uint8_t *)&pc_models[i] + (off)))
#define CAR8(i, n) (D_80152818[(i) * PC_CAR_SIZE + CAR_OFS(n)])

float func_8008B2E4(float max) { (void)max; return 0.0f; }   /* the random number CalcPolePositions draws in mode 3 */
void func_800A61B0(void *delta, void *out, void *matrix) { (void)delta; (void)matrix; memset(out, 0, 12); }
static int g_hs_slot = -1;
uint32_t high_scores_display(uint32_t sound, uint32_t slot, uint32_t one, uint8_t mode, float a, float b, float c)
{
    (void)sound; (void)one; (void)mode; (void)a; (void)b; (void)c;
    g_hs_slot = (int)slot;
    return 77;
}

void func_8039156C(void);
void func_800EC190(int8_t track);
void CalcPolePositions(void);
void func_800EC914(void);
int32_t stat_lap_split(int32_t sound, int32_t slot, float *position, uint8_t mode);

/* Slots from `from` on hold the sentinel in every byte. */
static int untouched_from(int from)
{
    for (int i = from; i < PC_LINK_SLOTS; i++)
        for (int k = 0; k < 8; k++)
            if (RAW(i, k) != SENTINEL)
                return 0;
    return 1;
}

static void test_layout(void)
{
    CHECK(PC_LINK_SLOTS == 16 && PC_MAX_LINKS == 16);   /* this test's build: more than the game's 13 */
    CHECK(sizeof(struct pc_link) == 8 && sizeof pc_links == 16 * 8);
    CHECK(offsetof(struct pc_link, track) == 0 && offsetof(struct pc_link, car) == 1);
    CHECK(offsetof(struct pc_link, color) == 2 && offsetof(struct pc_link, place) == 5);
    CHECK(offsetof(struct pc_link, flags) == 6 && offsetof(struct pc_link, owner) == 7);
}

/* func_8039156C: each of the 13 slots' car is its index; nothing else changes. */
static void test_ids(void)
{
    memset(pc_links, SENTINEL, sizeof pc_links);
    func_8039156C();
    for (int i = 0; i < PC_N64_LINK_SLOTS; i++) {
        CHECK(RAW(i, 1) == i && PC_LINK_CAR(i) == i);
        CHECK(RAW(i, 0) == SENTINEL && RAW(i, 2) == SENTINEL && RAW(i, 7) == SENTINEL);
    }
    CHECK(untouched_from(PC_N64_LINK_SLOTS));
    CHECK(*(int32_t *)D_803BA230 == -1 && (int8_t)D_803BA230[5] == -1 && D_803BA230[4] == 0);
}

/* func_800EC190: the race's 6 slots get the track, their place, flags 0xB0 and owner 0; the car and colours stay. */
static void test_reset(void)
{
    memset(pc_links, SENTINEL, sizeof pc_links);
    func_800EC190(5);
    CHECK(D_801543CA == 6 && D_8014978C == 5);
    for (int i = 0; i < 6; i++) {
        CHECK(RAW(i, 0) == 5 && RAW(i, 5) == i && RAW(i, 6) == 0xB0 && RAW(i, 7) == 0);
        CHECK(RAW(i, 1) == SENTINEL && RAW(i, 2) == SENTINEL && RAW(i, 3) == SENTINEL && RAW(i, 4) == SENTINEL);
    }
    CHECK(untouched_from(6));
}

/* CalcPolePositions for a race of 2 players and 2 drones: the humans start behind the drones (owner 6, places 2
 * and 3), the drones in front (owner 0, places 0 and 1), the last 2 of the 6 are empty (owner 7, flags 0). */
static void test_poles(void)
{
    static const unsigned char place[4] = {2, 3, 0, 1}, owner[6] = {6, 6, 0, 0, 7, 7};

    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = 2, D_80142724 = 2, D_8014A110 = 0, D_8014978C = 3;
    CalcPolePositions();
    CHECK(D_801543CA == 4);
    for (int i = 0; i < 6; i++) {
        CHECK(PC_LINK_TRACK(i) == 3 && PC_LINK_OWNER(i) == owner[i] && PC_LINK_FLAGS(i) == (i < 4 ? 0xB0 : 0));
        CHECK(PC_LINK_PLACE(i) == (i < 4 ? place[i] : SENTINEL) && PC_LINK_CAR(i) == SENTINEL);
    }
    CHECK(untouched_from(6));
}

/* func_800EC914 on test_poles' roster: 4 active cars of 6 slots; the sentinel slots after them, whose flags have
 * LINK_ACTIVE, are not counted. */
static void test_count(void)
{
    memset(pc_models, 0x55, sizeof pc_models);
    memset(D_80152818, 0x55, sizeof D_80152818);
    func_800EC914();
    CHECK(D_80152744 == 4);
    for (int i = 0; i < 4; i++) {
        CHECK(MODEL16(i, 0x7C6) == i && MODEL16(i, 0x7C8) == 1);                 /* slot, in_game */
        CHECK(MODEL16(i, 0x7CA) == (i >= 2) && MODEL8(i, 0x7CC) == (i < 2 ? 2 : 1));   /* we_control, HUMAN/DRONE */
        CHECK(CAR8(i, 0x35B) == i && CAR8(i, 0xEE) == PC_LINK_PLACE(i));           /* place, the link's +5 */
    }
    for (int i = 4; i < 6; i++)
        CHECK(MODEL16(i, 0x7C8) == 0 && MODEL16(i, 0x7CA) == 0 && MODEL8(i, 0x7CC) == 0);
    CHECK(D_80153FD2 == 2 && D_80143A40[0] == 0 && D_80143A40[1] == 1);              /* the humans */
    CHECK(D_80152768 == 2 && D_801527D8[0] == 2 && D_801527D8[1] == 3);              /* the drones */
    CHECK(D_8015274C == 2 && D_80152808[0] == 2 && D_80152808[1] == 3);              /* ours */
    CHECK(untouched_from(6));
}

/* stat_lap_split goes on only for a human's slot (owner 6), read where the alias D_80153E8F reached it. */
static void test_alias(void)
{
    float pos[3] = {0, 0, 0};

    D_8010FFC0 = 1;
    g_hs_slot = -1;
    CHECK(stat_lap_split(1, 2, pos, 0) == -1 && g_hs_slot == -1);   /* a drone */
    CHECK(stat_lap_split(1, 1, pos, 0) == 77 && g_hs_slot == 1);    /* a human */
    pc_links[1].owner = 0;
    CHECK(stat_lap_split(1, 1, pos, 0) == -1);
}

int main(void)
{
    test_layout();
    test_ids();
    test_reset();
    test_poles();
    test_count();
    test_alias();
    return TEST_DONE();
}
