/* port/assets/tests/test_cars.c: the car record (player_array, 0x468 bytes a car on the PC) and the model record
 * (D_8014A250, 0x808 bytes, its N64 layout) through the game's own code (docs/port/CARS.md): init_cars
 * (world_physics_tick), StartEnvironment and StartCar with every visual starter, the car visuals' stop loop, the
 * environment visual pool, the code that reaches cars at raw offsets or strides, and the model set-up from the
 * car physics descriptors, whose pointers are u32 window addresses, and the physics that read them (the torque
 * func_800E2D18, func_800E1500, func_800E2C70; docs/port/LAYOUT.md 5); race_init_helper reads a car's node handle's
 * low half; the brake lights (func_8008BEA4) read their car's slot from the visual. The car array is host storage,
 * as in the
 * game (own storage); the model array, the checkpoint block and the tune and torque tables sit at their N64
 * addresses in an RDRAM window the test maps (exit 77, skipped, if it can't), as the game's arena does.
 * game: src/blob/world_physics_tick.c src/blob/func_800B27E4.c src/blob/func_800EC270.c
 * game: src/blob/music_control.c src/blob/groups/audio_frame_update/startcar.c src/blob/save_load_data.c
 * game: src/blob/func_800AC8D4.c src/blob/groups/audio_frame/group.c src/blob/groups/audio_frame_update/group.c
 * game: src/ovl_b/func_8038C910.c src/blob/func_800B1F30.c
 * game: src/blob/func_800B2BDC.c src/blob/func_80090284.c src/blob/entity_transform_apply.c
 * game: src/blob/func_800C4078.c src/blob/func_800E9234.c src/blob/func_800CF604.c src/blob/gameover_validOption.c
 * game: src/blob/car_select_handler.c src/blob/func_800CFCA8.c src/blob/func_800FBE60.c
 * game: src/blob/groups/car_resistance/group.c src/blob/func_800D0894.c src/blob/func_800E313C.c
 * game: src/blob/func_800E2D18.c src/blob/func_800E1500.c src/blob/func_800E2C70.c src/blob/race_init_helper.c
 * game: src/blob/func_8008BEA4.c
 */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the PC layouts */

/* The car record on the PC: the N64 fields in order, its 21 visuals (struct pc_visual, 0x20 bytes) and its input
 * record pointer pointer-sized. */
struct car {
    uint8_t head[0x110];
    struct pc_visual visuals[21];
    uint8_t mid[0x380 - 0x308];
    void *input;
    uint8_t tail[0x3B8 - 0x384];
};

#define CF(c, n, T) (*(T *)((uint8_t *)(c) + CAR_OFS(n)))   /* the car field at N64 offset n */
#define MF(m, n, T) (*(T *)((uint8_t *)(m) + (n)))          /* the model field at offset n (N64 layout) */
/* Names defined here in assembly (absolute N64 addresses, or offsets into the car array) are for the game's
 * objects only: this file reaches that data by its address, since the assembler can't take a GOT reference to
 * a symbol it defines with .set. */
#define ABS(name, addr) __asm__(".globl " #name "\n.set " #name ", " #addr)
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
#define W32(p) ((uint32_t)(uintptr_t)(p))

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct car D_80152818[6] __attribute__((aligned(16)));
ALIAS(player_array, D_80152818, 0);
ALIAS(D_80152907, D_80152818, 0xEF);       /* names inside player_array, as gen_data places them */
ALIAS(D_80152B70, D_80152818, 0x400);      /* N64 +0x358 */
ALIAS(D_80152BD0, D_80152818, 0x468);      /* car 1, and its +8 and +0x50 */
ALIAS(D_80152BD8, D_80152818, 0x470);
ALIAS(D_80152C20, D_80152818, 0x4B8);
ALIAS(D_80152900, D_80152818, 0xE8);       /* a car's flags, seen as an array of cars */

struct pc_model pc_models[PC_MODEL_SLOTS];    /* model[], the model records (pc_game_types.h), which data.c defines */
#define MODEL(i) ((uint8_t *)&pc_models[i])
ABS(D_80151CE8, 0x80151CE8);               /* the checkpoint block (Route at +0) and two names inside it */
ABS(D_80151CEE, 0x80151CEE);
ABS(D_80151D18, 0x80151D18);
#define D_80151CE8 ((uint8_t *)(uintptr_t)0x80151CE8)
ABS(D_80139320, 0x80139320);               /* gCarParts, 0x40 bytes a car */
#define PARTS(i) ((uint32_t *)(uintptr_t)0x80139320 + (i) * 16)
ABS(D_801116D0, 0x801116D0);               /* the tune table, 44 bytes an entry */
ABS(D_8011F724, 0x8011F724);               /* the torque curve */
ABS(D_8011F814, 0x8011F814);               /* the tire records after it (car_resistance names them) */
#define TUNES ((float *)(uintptr_t)0x801116D0)
#define DESC_AT 0x80300000u                /* the physics descriptors (0xB4 bytes) and tire curves, as game data */
#define CURVES_AT 0x80301000u

void *D_80110D08[13];                      /* Car *[]: own storage, pointing into the game arena */
uint8_t D_8014A118[6 * 80] __attribute__((aligned(16)));   /* the input records */
struct pc_link pc_links[PC_LINK_SLOTS];    /* gLink, the race roster (pc_game_types.h) */
int8_t D_8011103C[7][13], D_80111080[7][13], D_8011119C[7][13], D_80111230[7][13], D_8011157C[7][13];
float D_8011F844[13][4];                   /* collision box: front, rear, side, height */
int8_t D_80146180[4];
int16_t D_8014A108, D_8015274C, D_80153F08, D_80153F24, D_80153F40;
int8_t D_80152015, D_80152744;
volatile int16_t D_80153E84, D_80153FD2, D_801543CA;
uint32_t state_word_a;
ALIAS(D_801174B4, state_word_a, 0);
int32_t gameplay_mode;
ALIAS(D_8014A110, gameplay_mode, 0);
int16_t active_player_count, D_80151AD0;
float D_801543CC = 12.5f;                  /* gModelTime */
volatile float D_8002EB90 = 3.25f;         /* gSeconds */
int8_t D_8014978C, D_80156994, gCarTeam[8];
uint8_t D_8011B57C[16], D_8011B58C[16], D_8011B59C[16], D_8011B5AC[16], D_8011AD8C[4], D_8011B558[5][4];
float D_8011AD90[4][3];
uint16_t D_80161368[16], D_801427C0[600], D_8014296A, D_8014296C, D_8014296E;
ABS(D_8014254C, 0x8014254C);               /* gObjList + 0x... in the game; only an id is read */
uint8_t D_8013FEF4[6 * 24];                /* the car palette descriptors' data words (u32 window addresses) */
int8_t D_80111299[6 * 13];
float D_8011B4B8[8 * 3], D_80123C1C = 0.75f, D_803943A4[8][13][3];
uint8_t D_8012E708[64], D_8008BEA4;
uint8_t D_8015B268[32 * 88];               /* tPoly instances */
int16_t D_8012E66C, D_8012E678;
void *D_801391F0, *D_801392C8;
extern struct pc_visual D_80138880[100];   /* defined by func_800B2BDC.c */
int8_t D_80152574[8];
float D_801526E0[8], D_801526F8[8];
uint8_t D_801543D4;
float D_80152018, D_80152218[6][3];
int8_t D_80152031;
float D_80110DA4[13], D_80110E0C[13], D_80110E44[13], D_8011121C[13], D_80111274[13], D_801112D0[13];
int8_t D_8011128C[13 * 7], D_801114E4[13];
float D_80124158 = 1.0f, D_8012415C = 2.0f, D_80142764 = 3.0f, D_80124134 = 2.0f;
float D_80110EBC[16][6];
float D_80110EC0[16][6], D_801110C4[8][3];   /* the gear table (func_800E2C70), the torque scale */

/* ---------------------------------------------------------------- what the code under test calls */

static int16_t g_handle = 100;
static int g_loads[64], g_nloads;
static void *g_called_v[32];
static int g_ncalled;

void model_data_load(int16_t objnum, int32_t a, int32_t b)
{
    if (g_nloads < 64)
        g_loads[g_nloads++] = objnum;
}
void model_transform_setup(int16_t objnum, int32_t a, int32_t b) {}
int32_t save_slot_valid(int32_t obj, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) { return g_handle++; }
int32_t func_800AF5E0(int32_t a, int16_t b, int32_t c, uint32_t d, int16_t e, int16_t f, int32_t g) { return 77; }
static int16_t g_node_moved[8];
static int g_nnode;
void func_8008D6FC(int16_t objnum, void *a, void *b) { if (g_nnode < 8) g_node_moved[g_nnode++] = objnum; }
void update_game_data(void) {}
void func_800EB90C(void) {}
void func_80092484(int16_t a, int16_t b) {}
static float g_pose[16];
intptr_t func_8008B3A0(int16_t objnum) { return (intptr_t)g_pose; }
void music_seq_load(int16_t slot, int16_t camlink, int for_game) {}
static int g_npoly;
void *func_800A78BC(int nverts, float (*xyz)[3], uint16_t tex, void *color, uint16_t flags, int useTidx)
{
    return D_8015B268 + 88 * g_npoly++;
}
void func_800C4F68(int16_t a, int32_t *b, int32_t c) {}
/* The visual functions the starters install (their addresses are what the test checks). */
#define VFN(name) void name(void *v, int16_t op) { if (g_ncalled < 32) g_called_v[g_ncalled++] = v; }
VFN(func_8008BD9C) VFN(func_8008BC94) VFN(func_8008BB8C) VFN(func_8008BA84) VFN(func_8008B964)
VFN(anim_state_update) VFN(buffer_swap) VFN(entity_anim_texture) VFN(func_8038AA8C) VFN(entity_process_main)
VFN(entity_render_setup) VFN(recorder)
int32_t AnimateFrame(void) { return 0; }
void func_8038C910(int16_t slot);
void func_8038c910(int16_t slot) { func_8038C910(slot); }   /* StartCar's spelling of StartWeapon */
/* The physics descriptor's tire set-up: what car_resistance passes. */
static uint32_t g_tire_info[4];
static float g_tire_x[4];
static int g_ntire, g_nprev;
void func_800D09E8(void *tire_info, void *tire) { if (g_ntire < 4) g_tire_info[g_ntire++] = W32(tire_info); }
void track_preview_handler(void *m, void *tire, float x) { if (g_nprev < 4) g_tire_x[g_nprev++] = x; }
float func_800D0B14(void *m) { return 1.5f; }
float func_800D0AA0(void *m) { return 2.5f; }
float func_800D0A34(void *m) { return 0.25f; }

void world_physics_tick(void);
void music_control(void);
void func_800B1F30(int16_t slot);
void func_800B2BDC(void);
void *func_80090284(void);
void entity_transform_apply(void *node, int unlink);
void func_800C4078(int16_t slot);
void func_800E9234(void *car);
int32_t func_800CF604(int16_t slot);
int32_t gameover_validOption(int32_t arg0);
void car_select_handler(int16_t slot);
void func_800CFCA8(void);
void func_800FBE60(void);
void track_info_display(void *m);
void func_800D0894(void *m);
void func_800E313C(void *m, float a, float b);
int16_t func_800E2D18(void *m, int16_t rpm, int16_t throttle, int16_t *torquecurve);
void func_800E1500(void *m, float *in, float *out);
void func_800E2C70(void *m);
void race_init_helper(void);
void func_8008BEA4(void *v, int16_t flag);

/* ---------------------------------------------------------------- tests */

static void test_layout(void)
{
    int k;

    CHECK(sizeof(struct car) == PC_CAR_SIZE && sizeof(struct pc_visual) == VIS_OFS(0x18));
    for (k = 0; k < 21; k++) {
        size_t v = 0x110 + 0x18 * (size_t)k;
        CHECK(CAR_OFS(v) == offsetof(struct car, visuals[k].next));
        CHECK(CAR_OFS(v + 4) == offsetof(struct car, visuals[k].index));
        CHECK(CAR_OFS(v + 6) == offsetof(struct car, visuals[k].objnum));
        CHECK(CAR_OFS(v + 8) == offsetof(struct car, visuals[k].slot));
        CHECK(CAR_OFS(v + 0xC) == offsetof(struct car, visuals[k].data));
        CHECK(CAR_OFS(v + 0x10) == offsetof(struct car, visuals[k].timeStamp));
        CHECK(CAR_OFS(v + 0x14) == offsetof(struct car, visuals[k].func));
    }
    CHECK(CAR_OFS(0x308) == offsetof(struct car, mid) && CAR_OFS(0x380) == offsetof(struct car, input));
    CHECK(CAR_OFS(0x384) == offsetof(struct car, tail) && CAR_OFS(0x3B8) == sizeof(struct car));
    CHECK(CAR_OFS(0x3B8 + 0xF6) == sizeof(struct car) + 0xF6);
}

/* init_cars: clears both records of every car (all 0x468 bytes of a car) and sets them up from gLink. Cars 0
 * and 2 are humans: their input pointers are their input records, 80 bytes apart on the PC (POINTERS.md 10). */
static void test_init_cars(void)
{
    int k;
    struct car *c0 = &D_80152818[0], *c1 = &D_80152818[1], *c2 = &D_80152818[2];

    memset(D_80152818, 0xAA, sizeof D_80152818);
    memset(MODEL(0), 0x55, 0x808 * 3);
    D_801543CA = 3;
    D_8014A108 = 1;
    pc_links[0].car = 3, pc_links[0].place = 1, pc_links[0].flags = 0x80, pc_links[0].owner = 6;
    pc_links[1].car = 5, pc_links[1].place = 2, pc_links[1].flags = 0x80, pc_links[1].owner = 2;
    pc_links[2].car = 4, pc_links[2].place = 3, pc_links[2].flags = 0x80, pc_links[2].owner = 7;
    D_8014A118[66] = 0x21;                       /* the humans' input record flags (+0x42) */
    D_8014A118[2 * 80 + 66] = 0x42;
    D_8011F844[0][0] = 4.0f, D_8011F844[0][1] = 3.0f, D_8011F844[0][2] = 1.5f, D_8011F844[0][3] = 2.0f;
    D_80110D08[0] = (void *)(uintptr_t)DESC_AT;
    MF(D_80151CE8, 2, int16_t) = 0, MF(D_80151CE8, 8, int16_t) = 4;   /* Route first, count */
    world_physics_tick();

    CHECK(c0->input == (void *)D_8014A118 && c1->input == NULL && c2->input == (void *)(D_8014A118 + 2 * 80));
    CHECK(MF(MODEL(2), 9, int8_t) == 0x42 && MF(MODEL(2), 8, int8_t) == 4 && MF(MODEL(2), 0x7CC, int8_t) == 2);
    for (k = 0; k < 21; k++) {
        CHECK(c0->visuals[k].func == NULL && c0->visuals[k].objnum == -1 && c0->visuals[k].next == NULL);
        CHECK(c1->visuals[k].func == NULL && c1->visuals[k].objnum == -1 && c1->visuals[k].data == 0);
    }
    CHECK(c0->tail[sizeof c0->tail - 1] == 0 && c1->tail[sizeof c1->tail - 1] == 0);   /* the PC tail is cleared */
    CHECK(D_80152818[3].head[0] == 0xAA);
    CHECK(CF(c0, 0x35B, int8_t) == 0 && CF(c1, 0x35B, int8_t) == 1);                   /* slot */
    CHECK(CF(c0, 0xEE, int8_t) == 1 && CF(c1, 0xEE, int8_t) == 2);                     /* place */
    CHECK(CF(c0, 0x35E, int8_t) == 2 && CF(c0, 0x35C, int8_t) == -1 && CF(c0, 0x35D, int8_t) == 2);
    CHECK(CF(c0, 0x100, float) == -6.0f && CF(c1, 0x100, float) == -1.0f);              /* distance */
    CHECK(CF(c0, 0x104, float) == -100.0f && CF(c0, 0x309, int8_t) == 1 && CF(c0, 0x311, int8_t) == -1);
    /* func_800EC270 (the car's checkpoint state, past the visuals) */
    CHECK(CF(c0, 0x334, float) == 12.5f && CF(c0, 0x33A, int16_t) == -1 && CF(c0, 0xFA, int16_t) == -1);
    CHECK(CF(c0, 0x338, int16_t) == 0 && CF(c0, 0x314, float) == 0.0f);
    /* the models (N64 layout, in the window) */
    CHECK(MF(MODEL(0), 0, uint32_t) == DESC_AT && MF(MODEL(1), 0, uint32_t) == DESC_AT);
    CHECK(MF(MODEL(0), 0x7C6, int16_t) == 0 && MF(MODEL(1), 0x7C6, int16_t) == 1);
    CHECK(MF(MODEL(0), 0x7CC, int8_t) == 2 && MF(MODEL(1), 0x7CC, int8_t) == 1);       /* human, drone */
    CHECK(MF(MODEL(0), 8, int8_t) == 3 && MF(MODEL(0), 9, int8_t) == 0x21);           /* body, input flags */
    CHECK(MF(MODEL(1), 268, float) == 1.5f && MF(MODEL(1), 288, float) == -3.0f);      /* the drone's box */
    CHECK(MF(MODEL(0), 0x7DC, int8_t) == 1 && CF(c1, 0x33C, int16_t) == 0);
}

/* StartEnvironment: StartCar for every car in the game (StartFrame, StartSparks, StartWing, StartWingFlame,
 * StartWeapon, the brake lights and the tires), then StartShadow and StartArrow. */
static void start_all(int mode)
{
    int i, k;

    for (i = 0; i < 2; i++) {
        for (k = 0; k < 21; k++) {
            D_80152818[i].visuals[k].func = NULL;
            D_80152818[i].visuals[k].objnum = -1;
        }
        MF(MODEL(i), 0x7C8, int16_t) = 1;                  /* in_game */
        MF(MODEL(i), 0x7CA, int16_t) = 0;
        MF(MODEL(i), 8, uint8_t) = 1;                      /* body type */
        for (k = 0; k < 4; k++)
            PARTS(i)[7 + k] = (uint32_t)(40 + 10 * i + k);    /* tires[4] at +0x1C */
        for (k = 0; k < 5; k++)
            PARTS(i)[11 + k] = (uint32_t)(60 + 10 * i + k);   /* the sparks' ids at +0x2C */
        PARTS(i)[0] = (uint32_t)(20 + i);                   /* the frame object */
    }
    pc_links[0].owner = 6, pc_links[1].owner = 2;
    gameplay_mode = mode;
    state_word_a = 0;
    active_player_count = 2;
    D_80151AD0 = 2;
    g_npoly = 0;
    music_control();
}

static void check_started(int mode)
{
    int i, k;

    for (i = 0; i < 2; i++) {
        struct car *c = &D_80152818[i];
        struct pc_visual *v = c->visuals;
        CHECK(v[0].func == (void *)func_8008BEA4 && v[0].slot == i);                        /* brake lights */
        CHECK(v[1].func == (void *)func_8008BD9C && v[2].func == (void *)func_8008BC94);   /* sparks */
        CHECK(v[3].func == (void *)func_8008BB8C && v[4].func == (void *)func_8008BA84);
        CHECK(v[5].func == (void *)func_8008B964);
        for (k = 1; k <= 5; k++)
            CHECK(v[k].index == -1 && v[k].slot == i && v[k].objnum == (int16_t)(60 + 10 * i + k - 1));
        for (k = 0; k < 4; k++) {                                                             /* tires */
            CHECK(v[6 + k].func == (void *)anim_state_update && v[6 + k].slot == i && v[6 + k].index == k);
            CHECK(v[6 + k].objnum == (int16_t)(40 + 10 * i + k) && v[6 + k].data == 0);
            CHECK(v[6 + k].timeStamp == 12.5f);
        }
        CHECK(v[10].func == (void *)entity_process_main && v[10].slot == i && v[10].timeStamp == 12.5f);
        CHECK(v[14].objnum == 20 + i && v[14].slot == i);                                    /* the frame */
        CHECK(v[14].func == (void *)AnimateFrame);
        if (mode != 6) {
            for (k = 0; k < 2; k++) {
                CHECK(v[16 + k].func == (void *)buffer_swap && v[16 + k].slot == i && v[16 + k].data == (uint32_t)k);
                CHECK(v[18 + k].func == (void *)entity_anim_texture && v[18 + k].slot == i);
                CHECK(v[18 + k].data == (uint32_t)k && v[16 + k].objnum >= 100 && v[18 + k].objnum >= 100);
            }
            CHECK(v[20].func == NULL);
        } else {
            CHECK(v[16].func == NULL && v[17].func == NULL && v[18].func == NULL && v[19].func == NULL);
            CHECK(v[20].func == (void *)func_8038AA8C && v[20].slot == i && v[20].objnum == 77);
            CHECK(v[20].index == 0 && v[20].timeStamp == 3.25f && PARTS(i)[6] == 77);
        }
    }
    /* StartArrow: only the human (car 0), one arrow for the other window, the next visual cleared. */
    CHECK(D_80152818[0].visuals[11].func == (void *)entity_render_setup && D_80152818[0].visuals[11].index == 1);
    CHECK(D_80152818[0].visuals[12].func == NULL && D_80152818[0].visuals[12].objnum == -1);
    CHECK(D_80152818[1].visuals[11].func == NULL);
    CHECK(D_80152818[0].visuals[10].objnum == 0 && D_80152818[0].visuals[11].objnum == 1);   /* poly indices */
}

/* func_800B1F30: each visual's func is called with the visual; one without a func just has its model hidden. */
static void test_stop_visuals(void)
{
    struct car *c = &D_80152818[0];
    int k, n;

    CF(c, 0xF4, int32_t) = 0;
    for (k = 0; k < 21; k++) {
        c->visuals[k].func = (k % 3 == 0) ? NULL : (void (*)(void *, short))recorder;
        c->visuals[k].objnum = (int16_t)(200 + k);
    }
    g_ncalled = 0, g_nloads = 0;
    func_800B1F30(0);
    for (k = 0, n = 0; k < 21; k++) {
        if (k % 3) {
            CHECK(n < g_ncalled && g_called_v[n] == &c->visuals[k]);
            n++;
            CHECK(c->visuals[k].objnum == 200 + k);
        } else {
            CHECK(c->visuals[k].objnum == -1);
        }
    }
    CHECK(n == g_ncalled && g_nloads == 1 + 7 && g_loads[1] == 200 && g_loads[7] == 218);
}

/* The environment visual pool: same layout as a car's visual. */
static void test_env_pool(void)
{
    struct pc_visual *a, *b, *c;

    func_800B2BDC();
    CHECK(D_801392C8 == &D_80138880[0] && D_80138880[0].next == &D_80138880[1] && D_80138880[99].next == NULL);
    D_80138880[1].data = 0x1234, D_80138880[1].func = (void (*)(void *, short))recorder;
    a = func_80090284(), b = func_80090284(), c = func_80090284();
    CHECK(a == &D_80138880[0] && b == &D_80138880[1] && c == &D_80138880[2] && D_8012E66C == 3);
    CHECK(b->next == NULL && b->index == 0 && b->objnum == -1 && b->slot == 0 && b->data == 0 && b->func == NULL);
    CHECK(b->timeStamp == 0.0f && D_801392C8 == &D_80138880[3]);
    b->func = (void (*)(void *, short))recorder, b->objnum = 9;
    a->next = b, b->next = c, c->next = NULL, D_801391F0 = a;
    g_nloads = 0;
    entity_transform_apply(b, 1);
    CHECK(a->next == c && D_801392C8 == b && b->next == &D_80138880[3] && b->func == NULL && b->objnum == -1);
    CHECK(g_nloads == 1 && g_loads[0] == 9 && D_8012E66C == 2);
}

/* Code that reaches cars by raw offsets, strides, or GameCar's padding (m2c). */
static void test_raw(void)
{
    struct car *c = &D_80152818[1];
    int16_t *table = (int16_t *)(D_80151CE8 + 0x30);

    /* func_800C4078: car+0x33A = D_80151D18[car+0x33C + model+0x7E2 * 40] */
    CF(c, 0x33C, int16_t) = 3;
    MF(MODEL(1), 0x7E2, int16_t) = 2;
    table[3 + 2 * 40] = 1234;
    func_800C4078(1);
    CHECK(CF(c, 0x33A, int16_t) == 1234);
    /* func_800E9234 (a light flash on the car it gets) */
    CF(c, 0x35C, int8_t) = 4;
    state_word_a = 8;
    func_800E9234(c);
    CHECK(CF(c, 0x35E, int8_t) == 2 && CF(c, 0x35D, int8_t) == 2 && D_801526E0[4] == 5.0f && D_80152574[4] == 1);
    /* func_800CF604: a stride on D_80152B70 (car + 0x358) */
    MF(MODEL(1), 0x640, int8_t) = 0, MF(MODEL(1), 0x6C4, int16_t) = -1;
    MF(MODEL(1), 0x7EB, int8_t) = 1, MF(MODEL(1), 0x7DF, int8_t) = 0;
    CF(c, 0x358, int8_t) = 0;
    CHECK(func_800CF604(1) == 1);
    CF(c, 0x358, int8_t) = 1;
    CHECK(func_800CF604(1) == 0);
    /* gameover_validOption: a stride on D_80152907 (car + 0xEF) */
    gameplay_mode = 3, D_801543D4 = 1;
    CF(c, 0xEF, int8_t) = 0;
    CHECK(gameover_validOption(5) == 0);
    CF(c, 0xEF, int8_t) = 1;
    CHECK(gameover_validOption(5) == 1);
    /* car_select_handler: GameCar's pad0EC[0x26C/0x26D] are car + 0x358/0x359 */
    gameplay_mode = 0;
    CF(c, 0x358, int8_t) = 0, CF(c, 0x359, int8_t) = 0;
    MF(MODEL(1), 0x640, int8_t) = 1, MF(MODEL(1), 0x714, float) = 7.0f;
    car_select_handler(1);
    CHECK(CF(c, 0x358, int8_t) == 1 && CF(c, 0x10C, float) == 7.0f && CF(c, 0xED, int8_t) == -1);
    MF(MODEL(1), 0x714, float) = 11.0f;
    car_select_handler(1);
    CHECK(CF(c, 0x358, int8_t) == 2);
    /* func_800CFCA8: a car's flash timer (car + 0xEC, 0x309, 0x310, 0x311, 0x30C, 0x359), N64 layout model flags */
    memset(MODEL(0) + 0x7C8, 0, 2);
    MF(MODEL(1), 0x7C8, int16_t) = 1, MF(MODEL(1), 0x7D4, uint32_t) = 0xFF;
    CF(c, 0xEC, int8_t) = 1, CF(c, 0x309, int8_t) = 0, CF(c, 0x310, int8_t) = 0, CF(c, 0x311, int8_t) = 0;
    CF(c, 0x30C, float) = 9.0f, CF(c, 0x359, int8_t) = 0;
    MF(MODEL(1), 0x714, float) = 10.5f;
    func_800CFCA8();
    CHECK(CF(c, 0x311, int8_t) == 1 && CF(c, 0x310, int8_t) == 0 && MF(MODEL(1), 0x7D4, uint32_t) == 0xF7);
    /* func_800FBE60 (race start): each car's +0x108 cleared and its position (+8) copied out, at the car stride */
    D_80152744 = 3;
    for (int i = 0; i < 3; i++) {
        CF(&D_80152818[i], 0x108, float) = 1.0f;
        CF(&D_80152818[i], 0x8, float) = 10.0f * (float)i, CF(&D_80152818[i], 0xC, float) = 1.0f + (float)i;
        CF(&D_80152818[i], 0x10, float) = -2.0f * (float)i;
    }
    func_800FBE60();
    for (int i = 0; i < 3; i++) {
        CHECK(CF(&D_80152818[i], 0x108, float) == 0.0f);
        CHECK(D_80152218[i][0] == 10.0f * (float)i && D_80152218[i][1] == 1.0f + (float)i);
        CHECK(D_80152218[i][2] == -2.0f * (float)i);
    }
}

/* The model from its physics descriptor (window addresses in u32 fields), and the descriptor's tire curves. */
static void test_model(void)
{
    uint8_t *m = MODEL(2), *desc = (uint8_t *)(uintptr_t)DESC_AT;
    int i;

    memset(desc, 0, 0xB4);
    for (i = 0; i < 4; i++) {
        MF(desc, 0x60 + 4 * i, uint32_t) = CURVES_AT + 0x40 * (uint32_t)i;
        MF(desc, 0x78 + 12 * i, float) = 2.75f + (float)i;    /* tire_positions[i].x (+0x90 is set below) */
    }
    MF(desc, 0x50, float) = 1080.0f, MF(desc, 0x54, float) = 2.0f, MF(desc, 0x90, float) = 8.0f;
    MF(desc, 0xA4, float) = 3.0f;
    D_80110D08[4] = desc;
    memset(m, 0, 0x808);
    MF(m, 8, uint8_t) = 4;               /* car_type */
    MF(m, 11, int8_t) = 2;               /* tune_type */
    TUNES[2 * 11 + 3] = 0.5f;            /* tune +0xC */
    g_ntire = g_nprev = 0;
    track_info_display(m);
    CHECK(MF(m, 0, uint32_t) == DESC_AT && MF(m, 4, uint32_t) == W32(&TUNES[2 * 11]));
    CHECK(MF(m, 4, uint32_t) == 0x801116D0u + 2 * 44 && MF(m, 0x40C, uint32_t) == 0x8011F724u);
    CHECK(g_ntire == 4 && g_nprev == 4);
    for (i = 0; i < 4; i++)
        CHECK(g_tire_info[i] == CURVES_AT + 0x40 * (uint32_t)i);
    CHECK(g_tire_x[0] == -8.0f && g_tire_x[1] == -5.75f && g_tire_x[2] == 2.75f && g_tire_x[3] == 3.75f);
    /* func_800D0894 reads the descriptor through +0 */
    MF(m, 0x5BC, float) = 4.0f, MF(m, 0x5C8, float) = 2.0f;
    func_800D0894(m);
    CHECK(MF(m, 0x3C4, float) == 540.0f / 1080.0f * 2.0f);
    CHECK(MF(m, 0x3C8, float) == 2.0f * 1080.0f / (4.0f * (8.0f / 2.0f)));
    /* func_800E313C: the up-shift gear from tune +0xC and descriptor +0xA4 (both through u32 fields) */
    for (i = 0; i < 6; i++)
        D_80110EBC[0][i] = 1.0f;
    MF(m, 9, int8_t) = 0, MF(m, 0x41C, float) = 10.0f;    /* 10 * 0.5 * 3 = 15 */
    func_800E313C(m, 14.0f, 0.0f);
    CHECK(MF(m, 0x3F4, int16_t) == 4);
    func_800E313C(m, 16.0f, 0.0f);
    CHECK(MF(m, 0x3F4, int16_t) == 1);
    /* the torque: the curve, scaled by tune +8 (through the u32 at +4) and the table at the model's +0xC, +0xB */
    {
        static int16_t curve[12 * 10];
        for (i = 0; i < 12 * 10; i++)
            curve[i] = 100;
        TUNES[2 * 11 + 2] = 2.0f;        /* tune +8 */
        MF(m, 0xB, int8_t) = 1, MF(m, 0xC, int8_t) = 2;
        D_801110C4[2][1] = 1.5f;
        CHECK(func_800E2D18(m, 2000, 50, curve) == 300);
    }
    /* the descriptor's +0xC..+0x14 scale a vector (func_800E1500), tune +0xC and descriptor +0xA4 the gear force */
    {
        float in[3] = {1.0f, 2.0f, 3.0f}, out[3];
        MF(desc, 0xC, float) = 2.0f, MF(desc, 0x10, float) = 3.0f, MF(desc, 0x14, float) = 4.0f;
        func_800E1500(m, in, out);
        CHECK(out[0] == 2.0f && out[1] == 6.0f && out[2] == 12.0f);
        MF(m, 0x3F4, int16_t) = 2, MF(m, 9, int8_t) = 1;
        D_80110EC0[1][2] = 4.0f;
        func_800E2C70(m);
        CHECK(MF(m, 0x428, float) == 0.5f * 4.0f && MF(m, 0x42C, float) == 3.0f * 2.0f);
    }
}

/* race_init_helper (a battle race) moves the other cars' scene nodes by their handles' low halves. */
static void test_race_init(void)
{
    gameplay_mode = 2, active_player_count = 3;
    CF(&D_80152818[1], 0xF4, int32_t) = 0x42;
    CF(&D_80152818[2], 0xF4, int32_t) = 0x43;
    g_nnode = 0;
    race_init_helper();
    CHECK(g_nnode == 2 && g_node_moved[0] == 0x42 && g_node_moved[1] == 0x43);
    gameplay_mode = 0;
}

/* The brake lights (AnimateBrakeLights, func_8008BEA4) of a braking car take the brake colours into its palette
 * record (a u32 window address in D_8013FEF4): the car is the visual's slot (+0xC on the PC; +8 is its index). */
#define PALETTES_AT 0x80302000u
static void test_brake_lights(void)
{
    struct pc_visual v;
    uint16_t *pal = (uint16_t *)(uintptr_t)(PALETTES_AT + 320);
    int i, k;

    for (i = 0; i < 6; i++) {
        CF(&D_80152818[i], 0xE8, uint32_t) = i == 1 ? 0x1000 : 0;
        MF(D_8013FEF4, 24 * i, uint32_t) = PALETTES_AT + 0x200 * (uint32_t)i;
        memset((uint8_t *)pal + 0x200 * i, 0, 16);
    }
    for (k = 0; k < 8; k++)
        MF(D_8011B57C, 2 * k, uint16_t) = (uint16_t)(0x100 + k), MF(D_8011B58C, 2 * k, uint16_t) = (uint16_t)(0x200 + k);
    D_8014978C = 0;
    memset(&v, 0, sizeof v);
    v.index = 3, v.slot = 1;
    func_8008BEA4(&v, 1);
    for (i = 0; i < 6; i++)
        for (k = 0; k < 8; k++)
            CHECK(pal[0x100 * i + k] == (i == 1 ? 0x200 + k : 0));
    func_8008BEA4(&v, 0);
    CHECK(pal[0x100] == 0x100 && pal[0x107] == 0x107);
    CF(&D_80152818[1], 0xE8, uint32_t) = 0;
}

int main(void)
{
    if (!test_rdram()) {
        fprintf(stderr, "test_cars: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    pc_fix_absolute_got();
    test_layout();
    test_init_cars();
    start_all(0);
    check_started(0);
    start_all(6);
    check_started(6);
    test_stop_visuals();
    test_env_pool();
    test_raw();
    test_model();
    test_race_init();
    test_brake_lights();
    return TEST_DONE();
}
