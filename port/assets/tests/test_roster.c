/* port/assets/tests/test_roster.c: the race roster (gLink: pc_links, struct pc_link, port/include/pc_game_types.h)
 * through the game's own readers and writers, with more slots declared than the game walks (docs/port/DECOUPLE.md).
 * This test and its game objects are built with PC_MAX_LINKS 16 (CMakeLists.txt), so the storage has 16 slots
 * where the game build has 13. Every slot starts as a sentinel: the code must write and read the fields at their
 * N64 offsets in slots it walks and leave the others alone, whatever the storage's size.
 * func_8039156C names the 13 slots' cars; func_800EC190 resets the attract demo's cars; CalcPolePositions
 * (init_state_continue) fills them for a race; func_800EC914 (munge_gLink_data) counts the active ones into the
 * race's car count D_80152744 and builds the human and drone lists (pc_drones, pc_our_drones: PC_MAX_LINKS long,
 * docs/port/DECOUPLE.md 6); stat_lap_split reads a slot's owner through what was the alias D_80153E8F, +7 with a
 * stride of 8.
 * A race of more cars (docs/port/MORECARS.md 3.1), test_race_cars 12 (what pc_race_cars() returns, support.c): the
 * demo's 12 drones and its camera's car going round them; CalcPolePositions for 1 or 2 humans and 10 or 11 drones,
 * the Drones option capped at the race's cars, and the circuit kept at the N64's 6; slots an earlier race of more
 * cars filled, emptied; init_cars (world_physics_tick) with the humans' first distance behind every drone and the
 * records after the race's reset; LoadCars giving every drone of 12 a car of its own. And munge alone for 12 slots
 * (one human and 11 drones, one of them another node's, then the demo's 12): the lists in order, nothing after them
 * written, the model records after the race's cleared to the storage's end.
 * game: src/ovl_a/func_8039156C.c src/blob/func_800EC190.c src/blob/groups/update_game/init_state_continue.c
 * game: src/blob/func_800EC914.c src/blob/stat_lap_split.c src/blob/world_physics_tick.c
 * game: src/blob/groups/loadcars/root.c
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
volatile int16_t D_801543CA, D_80152768, D_80153FD2, D_80153E84;
int16_t D_8014A108, D_80142724, D_8015274C, D_80143A40[16];
short pc_drones[PC_MAX_LINKS], pc_our_drones[PC_MAX_LINKS];   /* the drone lists (pc_game_types.h) */
#define LIST_SENTINEL ((short)0x5A5A)
int16_t D_8011F020[16], D_8011F040[16], D_80153F08, D_80153F24, D_80153F40, D_80142B08[13];
int32_t D_8014A110, D_80143FF4;
uint32_t state_word_a, D_801174B4, D_8002E580[183];
uint8_t D_801543D4, D_80154450[6 * 0x4C], D_803BA230[16 * 12] __attribute__((aligned(4)));
uint8_t D_8014A118[6 * 80] __attribute__((aligned(16)));   /* the input records, 80 bytes each on the PC */
int8_t D_80152015, D_80146180[16], D_80111604[13], D_80111648[13], D_8011168C[13];
int8_t D_8011103C[17][13], D_80111080[17][13], D_8011119C[17][13], D_80111230[17][13], D_8011157C[17][13];
float D_8011F844[13][4];
struct { int16_t f0, first, f4, f6, count; } D_80151CE8;
void *D_80110D08;
struct pc_model pc_models[PC_MODEL_SLOTS];    /* the model records (pc_game_types.h), N64 layout */
struct pc_car pc_cars[PC_CAR_SLOTS];   /* the car records (pc_game_types.h), PC layout */

#define MODEL16(i, off) (*(int16_t *)((uint8_t *)&pc_models[i] + (off)))
#define MODEL8(i, off) (*(int8_t *)((uint8_t *)&pc_models[i] + (off)))
#define CAR8(i, n) (((uint8_t *)pc_cars)[(i) * PC_CAR_SIZE + CAR_OFS(n)])
#define CARF(i, n) (*(float *)((uint8_t *)pc_cars + (i) * PC_CAR_SIZE + CAR_OFS(n)))

/* The random number CalcPolePositions draws for a circuit's first race and LoadCars for a drone's car: a counter, so
 * the draws differ and the code's retries end. */
static unsigned g_rand;
float func_8008B2E4(float max) { int m = (int)max; return m > 0 ? (float)(g_rand++ % (unsigned)m) : 0.0f; }
void func_800A61B0(void *delta, void *out, void *matrix) { (void)delta; (void)matrix; memset(out, 0, 12); }
static int g_hs_slot = -1;
uint32_t high_scores_display(uint32_t sound, uint32_t slot, uint32_t one, uint8_t mode, float a, float b, float c)
{
    (void)sound; (void)one; (void)mode; (void)a; (void)b; (void)c;
    g_hs_slot = (int)slot;
    return 77;
}
/* What init_cars and LoadCars call. */
void func_800B27E4(void *car) { (void)car; }
void func_800EC270(void *m, void *car) { (void)m; (void)car; }
uint32_t audio_output_setup(void *heap) { (void)heap; return 0x7FFFFFFF; }   /* the heap's free bytes */
int32_t func_80097694(int32_t wave, int8_t x) { (void)wave; (void)x; return -1; }
int32_t audio_frame_sync(int32_t wave, int32_t a, int32_t b, int32_t c, void *d)
{
    (void)a; (void)b; (void)c; (void)d;
    return 1000 + wave;                                   /* LoadWave: the wave's handle */
}
static int g_colored[16];
void func_800BB02C(int32_t slot, int32_t car, void *x) { (void)x; if (slot >= 0 && slot < 16) g_colored[slot] = car + 1; }

void func_8039156C(void);
void func_800EC190(int8_t track);
void CalcPolePositions(void);
void func_800EC914(void);
int32_t stat_lap_split(int32_t sound, int32_t slot, float *position, uint8_t mode);
void world_physics_tick(void);
void LoadCars(int32_t select);

/* Slots from `from` on hold the sentinel in every byte. */
static int untouched_from(int from)
{
    for (int i = from; i < PC_LINK_SLOTS; i++)
        for (int k = 0; k < 8; k++)
            if (RAW(i, k) != SENTINEL)
                return 0;
    return 1;
}

/* Slots from `from` to the storage's end are empty, as CalcPolePositions leaves the slots after a race's cars: the
 * track, owner 7 and flags 0, the car and colours and place as they were (the sentinel). */
static int empty_from(int from, int track)
{
    for (int i = from; i < PC_LINK_SLOTS; i++) {
        if (RAW(i, 0) != track || RAW(i, 6) != 0 || RAW(i, 7) != 7 || RAW(i, 5) != SENTINEL)
            return 0;
        for (int k = 1; k < 5; k++)
            if (RAW(i, k) != SENTINEL)
                return 0;
    }
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

/* func_800EC190, the attract demo, for n cars: its n slots get the track, their place, flags 0xB0 and owner 0; the
 * car and colours stay. Every slot after them up to PC_MAX_LINKS takes the loop's arm for slots past the count:
 * flags 0, owner the slot + 6, the rest as it was. The camera's car (input record 0's node and D_80150C04) is the
 * next one, round the n. */
static void demo(int n)
{
    int before = D_80150C04;

    memset(pc_links, SENTINEL, sizeof pc_links);
    test_race_cars = n;
    func_800EC190(5);
    CHECK(D_801543CA == n && D_8014978C == 5);
    for (int i = 0; i < n; i++) {
        CHECK(RAW(i, 0) == 5 && RAW(i, 5) == i && RAW(i, 6) == 0xB0 && RAW(i, 7) == 0);
        CHECK(RAW(i, 1) == SENTINEL && RAW(i, 2) == SENTINEL && RAW(i, 3) == SENTINEL && RAW(i, 4) == SENTINEL);
    }
    for (int i = n; i < PC_MAX_LINKS; i++) {
        CHECK(RAW(i, 6) == 0 && RAW(i, 7) == i + 6);
        CHECK(RAW(i, 0) == SENTINEL && RAW(i, 1) == SENTINEL && RAW(i, 5) == SENTINEL);
    }
    CHECK(D_80150C04 == (before + 1) % n && (int8_t)D_8014A118[0] == D_80150C04);
}

static void test_reset(void)
{
    demo(6);
    for (int k = 0; k < 7; k++)        /* the camera's car goes round the 6 */
        demo(6);
    for (int k = 0; k < 13; k++)       /* and round 12 */
        demo(12);
    test_race_cars = 6;
}

/* CalcPolePositions: for `humans` players and the Drones option `option`, in `mode`, the race holds `cars`: the
 * humans start behind the drones (owner 6, places after the drones'), the drones in front (owner 0, places 0, 1...),
 * every slot after them empty (owner 7, flags 0). */
static void poles(int humans, int option, int mode, int cars)
{
    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = humans, D_80142724 = option, D_8014A110 = mode, D_8014978C = 3, D_80154628 = 0;
    CalcPolePositions();
    CHECK(D_801543CA == cars);
    for (int i = 0; i < cars; i++) {
        CHECK(PC_LINK_TRACK(i) == 3 && PC_LINK_FLAGS(i) == 0xB0 && PC_LINK_CAR(i) == SENTINEL);
        CHECK(PC_LINK_OWNER(i) == (i < humans ? 6 : 0));
        if (mode != 3)
            CHECK(PC_LINK_PLACE(i) == (i < humans ? cars - humans + i : i - humans));
    }
    CHECK(empty_from(cars, 3));
}

static void test_poles(void)
{
    static const unsigned char place[4] = {2, 3, 0, 1}, owner[6] = {6, 6, 0, 0, 7, 7};

    /* a race of 2 players and 2 drones, as the N64 lays it out */
    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = 2, D_80142724 = 2, D_8014A110 = 0, D_8014978C = 3;
    CalcPolePositions();
    CHECK(D_801543CA == 4);
    for (int i = 0; i < 6; i++) {
        CHECK(PC_LINK_TRACK(i) == 3 && PC_LINK_OWNER(i) == owner[i] && PC_LINK_FLAGS(i) == (i < 4 ? 0xB0 : 0));
        CHECK(PC_LINK_PLACE(i) == (i < 4 ? place[i] : SENTINEL) && PC_LINK_CAR(i) == SENTINEL);
    }
    CHECK(empty_from(6, 3));
    poles(1, 5, 0, 6);                 /* the default: 5 drones */
    poles(1, 9, 0, 6);                 /* the N64's cap: 6 - humans */
    poles(2, 5, 0, 6);
    poles(1, 5, 4, 1);                 /* stunt: no drones */
}

/* With 12 cars a race: 11 drones with one human, 10 with two (the option capped at 12 - humans), the default
 * option's 5 still 6 cars; the circuit stays 6 (its first race's places are drawn, its later ones its standings');
 * and a race of fewer cars after one of 12 leaves no slot of the 12 active. */
static void test_poles12(void)
{
    test_race_cars = 12;
    poles(1, 11, 0, 12);
    poles(2, 10, 0, 12);
    poles(2, 11, 0, 12);
    poles(1, 15, 0, 12);
    poles(1, 5, 0, 6);
    poles(2, 3, 0, 5);
    poles(2, 11, 6, 2);                /* battle: no drones */
    poles(1, 5, 3, 6);                 /* the circuit: 5 drones, the N64's */
    poles(2, 5, 3, 6);                 /* 2 players: 4 */
    poles(1, 11, 3, 6);                /* whatever the option */
    for (int i = 1; i < 6; i++)        /* its first race's places, drawn: each one of 0-5 once */
        for (int j = 0; j < i; j++)
            CHECK(PC_LINK_PLACE(i) != PC_LINK_PLACE(j) && PC_LINK_PLACE(i) < 6);

    /* a later circuit race: the places are the standings', slots 0 and 1 swapped by the circuit's player */
    for (int k = 0; k < 6; k++)
        D_80154450[k * 0x4C + 1] = (uint8_t)(20 + k);
    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = 2, D_80142724 = 11, D_8014A110 = 3, D_80154628 = 1, D_801543D4 = 1;
    CalcPolePositions();
    CHECK(D_801543CA == 6);
    for (int i = 0; i < 6; i++)
        CHECK(PC_LINK_PLACE(i) == 20 + (i == 0 ? 1 : i == 1 ? 0 : i) && PC_LINK_FLAGS(i) == 0xB0
              && PC_LINK_OWNER(i) == (i < 2 ? 6 : 0));
    CHECK(empty_from(6, 3));
    D_80154628 = 0, D_801543D4 = 0;

    /* 12 cars, then 6: slots 6 to 11 of the 12 are empty again */
    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = 1, D_80142724 = 11, D_8014A110 = 0;
    CalcPolePositions();
    CHECK(D_801543CA == 12 && PC_LINK_FLAGS(11) == 0xB0);
    D_80142724 = 5;
    CalcPolePositions();
    CHECK(D_801543CA == 6 && PC_LINK_FLAGS(5) == 0xB0);
    for (int i = 6; i < PC_MAX_LINKS; i++)
        CHECK(PC_LINK_FLAGS(i) == 0 && PC_LINK_OWNER(i) == 7);
    test_race_cars = 6;
}

/* func_800EC914 on test_poles' roster: 4 active cars of 6 slots; the sentinel slots after them, whose flags have
 * LINK_ACTIVE, are not counted. */
static void test_count(void)
{
    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = 2, D_80142724 = 2, D_8014A110 = 0, D_8014978C = 3;
    CalcPolePositions();
    for (int i = 6; i < PC_LINK_SLOTS; i++)
        pc_links[i].flags = SENTINEL;
    memset(pc_models, 0x55, sizeof pc_models);
    memset(pc_cars, 0x55, sizeof pc_cars);
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
    CHECK(D_80152768 == 2 && pc_drones[0] == 2 && pc_drones[1] == 3);                /* the drones */
    CHECK(D_8015274C == 2 && pc_our_drones[0] == 2 && pc_our_drones[1] == 3);        /* ours */
}

/* func_800EC914 on a roster of 12: one human and 11 drones counted and listed. */
static void test_count12(void)
{
    test_race_cars = 12;
    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = 1, D_80142724 = 11, D_8014A110 = 0, D_8014978C = 3;
    CalcPolePositions();
    memset(pc_models, 0x55, sizeof pc_models);
    memset(pc_cars, 0x55, sizeof pc_cars);
    func_800EC914();
    CHECK(D_80152744 == 12 && D_80153FD2 == 1 && D_80143A40[0] == 0);
    CHECK(D_80152768 == 11 && D_8015274C == 11);
    for (int i = 0; i < 12; i++) {
        CHECK(MODEL16(i, 0x7C6) == i && MODEL16(i, 0x7C8) == 1 && MODEL8(i, 0x7CC) == (i < 1 ? 2 : 1));
        CHECK(CAR8(i, 0xEE) == (i == 0 ? 11 : i - 1));
        if (i > 0)
            CHECK(pc_drones[i - 1] == i && pc_our_drones[i - 1] == i);
    }
    test_race_cars = 6;
}

/* func_800EC914 alone for 12 cars, more than the N64's 6: slot 0 a human and slots 1-11 drones (slot 7 driven by another
 * node, owner 3: a drone, not ours), then the attract demo's 12 drones. Every active slot is counted and listed in
 * order; the lists' entries after the last stay as they were, and the model records after the race's 12 (to the
 * storage's 16) are cleared. */
static void test_munge12(void)
{
    for (int demo = 0; demo < 2; demo++) {
        int humans = demo ? 0 : 1, n = 0, ours = 0;

        memset(pc_links, SENTINEL, sizeof pc_links);
        memset(pc_models, 0x55, sizeof pc_models);
        memset(pc_cars, 0x55, sizeof pc_cars);
        for (int i = 0; i < PC_MAX_LINKS; i++)
            pc_drones[i] = pc_our_drones[i] = LIST_SENTINEL;
        for (int i = 0; i < 12; i++) {
            pc_links[i].flags = 0xB0;
            pc_links[i].place = (unsigned char)(11 - i);
            pc_links[i].owner = (unsigned char)(i < humans ? 6 : !demo && i == 7 ? 3 : 0);
        }
        D_801543CA = 12;
        func_800EC914();
        CHECK(D_80152744 == 12);
        for (int i = 0; i < 12; i++) {
            CHECK(MODEL16(i, 0x7C6) == i && MODEL16(i, 0x7C8) == 1);
            CHECK(MODEL8(i, 0x7CC) == (i < humans ? 2 : 1) && CAR8(i, 0xEE) == 11 - i);
        }
        for (int i = 12; i < PC_MODEL_SLOTS; i++)
            CHECK(MODEL16(i, 0x7C8) == 0 && MODEL8(i, 0x7CC) == 0 && MODEL16(i, 0x7CA) == 0);   /* owner 0xEE */
        CHECK(D_80153FD2 == humans && (demo || D_80143A40[0] == 0));
        for (int i = humans; i < 12; i++) {
            CHECK(pc_drones[n++] == i);
            if (demo || i != 7)
                CHECK(pc_our_drones[ours++] == i);
        }
        CHECK(D_80152768 == n && n == 12 - humans && D_8015274C == ours && ours == (demo ? 12 : 10));
        for (int i = n; i < PC_MAX_LINKS; i++)
            CHECK(pc_drones[i] == LIST_SENTINEL);
        for (int i = ours; i < PC_MAX_LINKS; i++)
            CHECK(pc_our_drones[i] == LIST_SENTINEL);
    }
}

/* stat_lap_split goes on only for a human's slot (owner 6), read where the alias D_80153E8F reached it. */
static void test_alias(void)
{
    float pos[3] = {0, 0, 0};

    pc_links[1].owner = 6, pc_links[2].owner = 0;
    D_8010FFC0 = 1;
    g_hs_slot = -1;
    CHECK(stat_lap_split(1, 2, pos, 0) == -1 && g_hs_slot == -1);   /* a drone */
    CHECK(stat_lap_split(1, 1, pos, 0) == 77 && g_hs_slot == 1);    /* a human */
    pc_links[1].owner = 0;
    CHECK(stat_lap_split(1, 1, pos, 0) == -1);
}

/* init_cars (world_physics_tick) on CalcPolePositions' roster for `humans` and `option`: every car's first distance
 * (+0x100), a human's behind every drone, -(the larger of 6 and the cars, less its slot), a drone's -1, -2... in its
 * order; the cars active; the records after them up to the storage's end reset (in_game, drone type 0; ours only
 * where the slot's owner is 0), whatever an earlier race left in them. */
static void init_cars(int humans, int option, int cars)
{
    int grid = cars > 6 ? cars : 6;

    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = humans, D_80142724 = option, D_8014A110 = 0, D_8014978C = 3;
    CalcPolePositions();
    CHECK(D_801543CA == cars);
    for (int i = 0; i < PC_LINK_SLOTS; i++)      /* a car type each (the tables' rows are 13 wide) */
        pc_links[i].car = (unsigned char)(i % 13);
    for (int i = 0; i < PC_MODEL_SLOTS; i++)     /* what an earlier race left: every record a drone in the race */
        MODEL16(i, 0x7C8) = 1, MODEL16(i, 0x7CA) = 1, MODEL8(i, 0x7CC) = 1;
    memset(pc_cars, 0x55, sizeof pc_cars);
    state_word_a = 0;
    world_physics_tick();
    CHECK(D_80152744 == cars);
    for (int i = 0; i < cars; i++) {
        CHECK(CARF(i, 0x100) == (i < humans ? -(float)(grid - i) : -(float)(i - humans + 1)));
        CHECK(MODEL16(i, 0x7C8) == 1 && MODEL16(i, 0x7C6) == i && MODEL8(i, 0x7CC) == (i < humans ? 2 : 1));
        CHECK(CAR8(i, 0x35B) == i && CAR8(i, 0xEE) == PC_LINK_PLACE(i));
    }
    for (int i = cars; i < PC_MODEL_SLOTS; i++)
        CHECK(MODEL16(i, 0x7C8) == 0 && MODEL16(i, 0x7CA) == 0 && MODEL8(i, 0x7CC) == 0);
}

static void test_init_cars(void)
{
    init_cars(1, 5, 6);                /* the N64's: the human at -6, the drones -1 to -5 */
    init_cars(2, 2, 4);                /* -6 and -5, -1 and -2 */
    test_race_cars = 12;
    init_cars(1, 5, 6);                /* 6 cars with the setting at 12: the N64's distances */
    init_cars(1, 11, 12);              /* the human at -12, the drones -1 to -11 */
    init_cars(2, 10, 12);              /* -12 and -11, the drones -1 to -10 */
    init_cars(1, 7, 8);                /* -8, -1 to -7 */
    test_race_cars = 6;
}

/* LoadCars after munge (the models' drone types): each drone slot of the race's cars gets a car type of its own
 * (none of an earlier slot's, the human's among them), its colours and its car file loaded; the human's chosen car
 * is loaded; every slot after the race's cars, to the 13, takes slot 0's car and wave. */
static void load_cars(int cars)
{
    memset(pc_links, SENTINEL, sizeof pc_links);
    D_8014A108 = 1, D_80142724 = 11, D_8014A110 = 0, D_8014978C = 3;
    CalcPolePositions();
    CHECK(D_801543CA == cars);
    func_800EC914();
    pc_links[0].car = 4;
    for (int i = 0; i < 13; i++)
        D_80111604[i] = (int8_t)(40 + i), D_80111648[i] = (int8_t)(60 + i), D_8011168C[i] = (int8_t)(80 + i);
    memset(D_80142B08, 0, sizeof D_80142B08);
    memset(g_colored, 0, sizeof g_colored);
    D_801174B4 = 0;
    LoadCars(0);
    for (int i = 0; i < cars; i++) {
        CHECK(PC_LINK_CAR(i) < 13 && D_80142B08[i] == 1000 + 88 + PC_LINK_CAR(i) && g_colored[i] == PC_LINK_CAR(i) + 1);
        for (int j = 0; j < i; j++)
            CHECK(PC_LINK_CAR(i) != PC_LINK_CAR(j));
        if (i > 0)
            CHECK(PC_LINK_COLOR0(i) == 40 + PC_LINK_CAR(i) && PC_LINK_COLOR2(i) == 80 + PC_LINK_CAR(i));
    }
    CHECK(PC_LINK_CAR(0) == 4);
    for (int i = cars; i < 13; i++)
        CHECK(PC_LINK_CAR(i) == 4 && D_80142B08[i] == 1000 + 88 + 4 && g_colored[i] == 0);
}

static void test_load_cars(void)
{
    load_cars(6);                      /* the N64's 6: the option capped at 5 drones */
    test_race_cars = 12;
    load_cars(12);
    test_race_cars = 6;
}

int main(void)
{
    test_layout();
    test_ids();
    test_reset();
    test_poles();
    test_poles12();
    test_count();
    test_count12();
    test_alias();
    test_init_cars();
    test_load_cars();
    test_munge12();
    return TEST_DONE();
}
