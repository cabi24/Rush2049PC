/* port/assets/tests/test_cars.c: the car record (pc_cars, N64 gGameCar, 0x468 bytes a car on the PC) and the
 * model record (pc_models, N64 D_8014A250, 0x808 bytes, its N64 layout) through the game's own code
 * (docs/port/CARS.md): init_cars
 * (init_cars), StartEnvironment and StartCar with every visual starter, the car visuals' stop loop, the
 * environment visual pool, the code that reaches cars at raw offsets or strides, and the model set-up from the
 * car physics descriptors, whose pointers are u32 window addresses, and the physics that read them (the torque
 * enginetorque, calcaa, transmission; docs/port/LAYOUT.md 5); Update_MDrive reads a car's node handle's
 * low half; the brake lights (AnimateBrakeLights) read their car's slot from the visual. The car and model arrays are
 * host storage, as in the game (PC data, docs/port/DECOUPLE.md); the checkpoint block and the tune and torque
 * tables sit at their N64 addresses in an RDRAM window the test maps (exit 77, skipped, if it can't), as the
 * game's arena does.
 * game: src/blob/init_cars.c src/blob/InitCarVisuals.c src/blob/InitCarPath.c
 * game: src/blob/StartEnvironment.c src/blob/groups/StartCar/startcar.c src/blob/StartFrame.c
 * game: src/blob/StartSparks.c src/blob/groups/StartWingFlame/group.c src/blob/groups/StartCar/group.c
 * game: src/ovl_b/StartWeapon.c src/blob/RemoveCar.c
 * game: src/blob/InitEnvs.c src/blob/GrabEnvEntry.c src/blob/ReleaseEnviron.c
 * game: src/blob/SyncMaxpathToLastCheckpoint.c src/blob/init_view3.c src/blob/AreWeSolid.c src/blob/gameover_validOption.c
 * game: src/blob/CheckCrash.c src/blob/BlinkResurrectingCars.c src/blob/InitCPTime.c
 * game: src/blob/groups/carsetup/group.c src/blob/steer_constants.c src/blob/find_best_gear.c
 * game: src/blob/enginetorque.c src/blob/calcaa.c src/blob/transmission.c src/blob/Update_MDrive.c
 * game: src/blob/AnimateBrakeLights.c src/blob/ZOID_SetPolyZ.c
 * game: src/blob/GetNextCheckpoint.c src/blob/CalcTireSideForce.c src/blob/interp.c
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
/* Names defined here in assembly (absolute N64 addresses, support.h's TEST_ABS, or offsets into the car array) are for
 * the game's objects only: this file reaches that data by its address, since the assembler can't take a GOT reference
 * to a symbol it defines with .set. */
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
#define W32(p) ((uint32_t)(uintptr_t)(p))

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_car pc_cars[PC_CAR_SLOTS];      /* gGameCar, the car records (pc_game_types.h), which data.c defines */
#define CARS ((struct car *)pc_cars)       /* in this file's view */

struct pc_model pc_models[PC_MODEL_SLOTS];    /* model[], the model records (pc_game_types.h), which data.c defines */
#define MODEL(i) ((uint8_t *)&pc_models[i])
TEST_ABS(gCheckpoints);                      /* the checkpoint block (Route at +0) and two names inside it */
TEST_ABS_IN(checkpoints_start_pt, gCheckpoints, 0x6);
TEST_ABS_IN(gCheckpoints_pathPts, gCheckpoints, 0x30);
#define gCheckpoints TEST_AT(gCheckpoints)
TEST_ABS(gCarParts);                         /* gCarParts, 0x40 bytes a car */
#define PARTS(i) ((uint32_t *)TEST_AT(gCarParts) + (i) * 16)
TEST_ABS(gTune);                             /* the tune table, 44 bytes an entry */
TEST_ABS(stdtorquecurve);                    /* the torque curve */
TEST_ABS(D_8011F814);                        /* the tire records after it (car_resistance names them) */
#define TUNES ((float *)TEST_AT(gTune))
#define DESC_AT TEST_SCRATCH                 /* the physics descriptors (0xB4 bytes) and tire curves, as game data */
#define CURVES_AT (TEST_SCRATCH + 0x1000u)

void *gCarModels[13];                      /* Car *[]: own storage, pointing into the game arena */
uint8_t gPlayers[6 * 80] __attribute__((aligned(16)));   /* the input records */
struct pc_link pc_links[PC_LINK_SLOTS];    /* gLink, the race roster (pc_game_types.h) */
int8_t gCarHandling[7][13], gCarEngines[7][13], gCarTires[7][13], gCarFrame[7][13], gTireRims[7][13];
float collsize[13][4];                   /* collision box: front, rear, side, height */
int8_t gDriveHulk[4];
int16_t gNumPlayers, num_our_drones, new_sched_to_use, sched_index, cur_drone;
int8_t gOneMoreCP, gNumActiveCars;
volatile int16_t sched_to_use, num_humans, gNumCars;
uint32_t gGameState;
int32_t gRace;
int16_t gNumPlayers, gNumWindows;
float gModelTime = 12.5f;                  /* gModelTime */
volatile float gSeconds = 3.25f;         /* gSeconds */
int8_t gTrackNum, expansion_pak_present, gCarTeam[8];
uint8_t taillight_colors_dark[16], taillight_colors_dim[16], taillight_colors_bright[16], headlight_colors_night[16], gShadowColor[4], arrowColors[5][4];
float gDummyVtx[4][3];
uint16_t gTexList[16], gObjList[600], gObjList_rocket_a, gObjList_rocket_b, gObjList_rocket_c;
TEST_ABS(gObjList_biased);                   /* an id in gObjList in the game; only the id is read */
uint8_t car_palettes_palBase[6 * 24];                /* the car palette descriptors' data words (u32 window addresses) */
int8_t gCarWings_players[6 * 13];
float wingOffsets_y[8 * 3], wing_flame_height = 0.75f, weaponOffsets[8][13][3];
uint8_t ObjInstanceList_mat[64];
uint8_t PolyInstanceList[32 * 88];               /* tPoly instances */
int16_t gEnvCount, gEnvCountMax;
void *gEnvList, *gFreeVis;
extern struct pc_visual gEnvirons[100];   /* defined by InitEnvs.c */
int8_t view3_switch[8];
float rear_y[8], rear_z[8];
uint8_t gCircuitPlayer;
float gFirstPlaceTime, pc_cp_last_pos[PC_MAX_LINKS][3];
int8_t gInFirstPlace;
float gCarBraking[13], gCollisionMass[13], gCrashThreshold[13], gTireSideForce[13], carDurability[13], gWingSizes[13];
int8_t gCarWings[13 * 7], gCarDifferential[13];
float carsetup_durability_base = 1.0f, carsetup_crash_threshold_scale = 2.0f, gGravity = 3.0f, steer_deg_to_rad = 2.0f;
float carTransGearRatio[16][6];
float carTransGearRatio_biased[16][6], carEngineTorque[8][3];   /* the gear table (transmission), the torque scale */

/* ---------------------------------------------------------------- what the code under test calls */

static int16_t g_handle = 100;
static int g_loads[64], g_nloads;
static void *g_called_v[32];
static int g_ncalled;

void MBOX_HideObject(int16_t objnum, int32_t a, int32_t b)
{
    if (g_nloads < 64)
        g_loads[g_nloads++] = objnum;
}
void MBOX_ShowObject(int16_t objnum, int32_t a, int32_t b) {}
/* the car objects: the weapon's (StartWeapon, flags 0x2000) 77, the others numbered from g_handle */
int32_t InitCarObj(int32_t obj, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    return c == 0x2000 ? 77 : g_handle++;
}
static int16_t g_node_moved[8];
static int g_nnode;
void ZOID_UpdateObject(int16_t objnum, void *a, void *b) { if (g_nnode < 8) g_node_moved[g_nnode++] = objnum; }
void update_game_data(void) {}
void CheckCameraView(void) {}
void InitWingSmoke(int16_t a, int16_t b) {}
static float g_pose[16];
intptr_t ZOID_FindMatrix(int16_t objnum) { return (intptr_t)g_pose; }
void CreateCar(int16_t slot, int16_t camlink, int for_game) {}
static int g_npoly;
void *ZOID_NewPoly(int nverts, float (*xyz)[3], uint16_t tex, void *color, uint16_t flags, int useTidx)
{
    return PolyInstanceList + 88 * g_npoly++;
}
void SetResurrectionData(int16_t a, int32_t *b, int32_t c) {}
/* The visual functions the starters install (their addresses are what the test checks). */
#define VFN(name) void name(void *v, int16_t op) { if (g_ncalled < 32) g_called_v[g_ncalled++] = v; }
VFN(AnimateLFSpark) VFN(AnimateLRSpark) VFN(AnimateRFSpark) VFN(AnimateRRSpark) VFN(AnimateBSpark)
VFN(AnimateTire) VFN(AnimateWing) VFN(AnimateWingFlame) VFN(AnimateWeapon) VFN(AnimateShadow)
VFN(AnimateArrow) VFN(recorder)
int32_t AnimateFrame(void) { return 0; }
void StartWeapon(int16_t slot);
/* The physics descriptor's tire set-up: what car_resistance passes. */
static uint32_t g_tire_info[4];
static float g_tire_x[4];
static int g_ntire, g_nprev;
void copy_tire_info(void *tire_info, void *tire) { if (g_ntire < 4) g_tire_info[g_ntire++] = W32(tire_info); }
void tire_constants(void *m, void *tire, float x) { if (g_nprev < 4) g_tire_x[g_nprev++] = x; }
float CalcMass(void *m) { return 1.5f; }
float CalcOffroadTires(void *m) { return 2.5f; }
float CalcDrift(void *m) { return 0.25f; }

void init_cars(void);
void StartEnvironment(void);
void RemoveCar(int16_t slot);
void InitEnvs(void);
void *GrabEnvEntry(void);
void ReleaseEnviron(void *node, int unlink);
void SyncMaxpathToLastCheckpoint(int16_t slot);
void init_view3(void *car);
int32_t AreWeSolid(int16_t slot);
int32_t gameover_validOption(int32_t arg0);
void CheckCrash(int16_t slot);
void BlinkResurrectingCars(void);
void InitCPTime(void);
void carsetup(void *m);
void steer_constants(void *m);
void find_best_gear(void *m, float a, float b);
int16_t enginetorque(void *m, int16_t rpm, int16_t throttle, int16_t *torquecurve);
void calcaa(void *m, float *in, float *out);
void transmission(void *m);
void Update_MDrive(void);
void AnimateBrakeLights(void *v, int16_t flag);

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
    struct car *c0 = &CARS[0], *c1 = &CARS[1], *c2 = &CARS[2];

    memset(pc_cars, 0xAA, sizeof pc_cars);
    memset(MODEL(0), 0x55, 0x808 * 3);
    gNumCars = 3;
    gNumPlayers = 1;
    pc_links[0].car = 3, pc_links[0].place = 1, pc_links[0].flags = 0x80, pc_links[0].owner = 6;
    pc_links[1].car = 5, pc_links[1].place = 2, pc_links[1].flags = 0x80, pc_links[1].owner = 2;
    pc_links[2].car = 4, pc_links[2].place = 3, pc_links[2].flags = 0x80, pc_links[2].owner = 7;
    gPlayers[66] = 0x21;                       /* the humans' input record flags (+0x42) */
    gPlayers[2 * 80 + 66] = 0x42;
    collsize[0][0] = 4.0f, collsize[0][1] = 3.0f, collsize[0][2] = 1.5f, collsize[0][3] = 2.0f;
    gCarModels[0] = (void *)(uintptr_t)DESC_AT;
    MF(gCheckpoints, 2, int16_t) = 0, MF(gCheckpoints, 8, int16_t) = 4;   /* Route first, count */
    init_cars();

    CHECK(c0->input == (void *)gPlayers && c1->input == NULL && c2->input == (void *)(gPlayers + 2 * 80));
    CHECK(MF(MODEL(2), 9, int8_t) == 0x42 && MF(MODEL(2), 8, int8_t) == 4 && MF(MODEL(2), 0x7CC, int8_t) == 2);
    for (k = 0; k < 21; k++) {
        CHECK(c0->visuals[k].func == NULL && c0->visuals[k].objnum == -1 && c0->visuals[k].next == NULL);
        CHECK(c1->visuals[k].func == NULL && c1->visuals[k].objnum == -1 && c1->visuals[k].data == 0);
    }
    CHECK(c0->tail[sizeof c0->tail - 1] == 0 && c1->tail[sizeof c1->tail - 1] == 0);   /* the PC tail is cleared */
    CHECK(CARS[3].head[0] == 0xAA);
    CHECK(CF(c0, 0x35B, int8_t) == 0 && CF(c1, 0x35B, int8_t) == 1);                   /* slot */
    CHECK(CF(c0, 0xEE, int8_t) == 1 && CF(c1, 0xEE, int8_t) == 2);                     /* place */
    CHECK(CF(c0, 0x35E, int8_t) == 2 && CF(c0, 0x35C, int8_t) == -1 && CF(c0, 0x35D, int8_t) == 2);
    CHECK(CF(c0, 0x100, float) == -6.0f && CF(c1, 0x100, float) == -1.0f);              /* distance */
    CHECK(CF(c0, 0x104, float) == -100.0f && CF(c0, 0x309, int8_t) == 1 && CF(c0, 0x311, int8_t) == -1);
    /* InitCarPath (the car's checkpoint state, past the visuals) */
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
            CARS[i].visuals[k].func = NULL;
            CARS[i].visuals[k].objnum = -1;
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
    gRace = mode;
    gGameState = 0;
    gNumPlayers = 2;
    gNumWindows = 2;
    g_npoly = 0;
    StartEnvironment();
}

static void check_started(int mode)
{
    int i, k;

    for (i = 0; i < 2; i++) {
        struct car *c = &CARS[i];
        struct pc_visual *v = c->visuals;
        CHECK(v[0].func == (void *)AnimateBrakeLights && v[0].slot == i);                        /* brake lights */
        CHECK(v[1].func == (void *)AnimateLFSpark && v[2].func == (void *)AnimateLRSpark);   /* sparks */
        CHECK(v[3].func == (void *)AnimateRFSpark && v[4].func == (void *)AnimateRRSpark);
        CHECK(v[5].func == (void *)AnimateBSpark);
        for (k = 1; k <= 5; k++)
            CHECK(v[k].index == -1 && v[k].slot == i && v[k].objnum == (int16_t)(60 + 10 * i + k - 1));
        for (k = 0; k < 4; k++) {                                                             /* tires */
            CHECK(v[6 + k].func == (void *)AnimateTire && v[6 + k].slot == i && v[6 + k].index == k);
            CHECK(v[6 + k].objnum == (int16_t)(40 + 10 * i + k) && v[6 + k].data == 0);
            CHECK(v[6 + k].timeStamp == 12.5f);
        }
        CHECK(v[10].func == (void *)AnimateShadow && v[10].slot == i && v[10].timeStamp == 12.5f);
        CHECK(v[14].objnum == 20 + i && v[14].slot == i);                                    /* the frame */
        CHECK(v[14].func == (void *)AnimateFrame);
        if (mode != 6) {
            for (k = 0; k < 2; k++) {
                CHECK(v[16 + k].func == (void *)AnimateWing && v[16 + k].slot == i && v[16 + k].data == (uint32_t)k);
                CHECK(v[18 + k].func == (void *)AnimateWingFlame && v[18 + k].slot == i);
                CHECK(v[18 + k].data == (uint32_t)k && v[16 + k].objnum >= 100 && v[18 + k].objnum >= 100);
            }
            CHECK(v[20].func == NULL);
        } else {
            CHECK(v[16].func == NULL && v[17].func == NULL && v[18].func == NULL && v[19].func == NULL);
            CHECK(v[20].func == (void *)AnimateWeapon && v[20].slot == i && v[20].objnum == 77);
            CHECK(v[20].index == 0 && v[20].timeStamp == 3.25f && PARTS(i)[6] == 77);
        }
    }
    /* StartArrow: only the human (car 0), one arrow for the other window, the next visual cleared. */
    CHECK(CARS[0].visuals[11].func == (void *)AnimateArrow && CARS[0].visuals[11].index == 1);
    CHECK(CARS[0].visuals[12].func == NULL && CARS[0].visuals[12].objnum == -1);
    CHECK(CARS[1].visuals[11].func == NULL);
    CHECK(CARS[0].visuals[10].objnum == 0 && CARS[0].visuals[11].objnum == 1);   /* poly indices */
}

/* RemoveCar: each visual's func is called with the visual; one without a func just has its model hidden. */
static void test_stop_visuals(void)
{
    struct car *c = &CARS[0];
    int k, n;

    CF(c, 0xF4, int32_t) = 0;
    for (k = 0; k < 21; k++) {
        c->visuals[k].func = (k % 3 == 0) ? NULL : (void (*)(void *, short))recorder;
        c->visuals[k].objnum = (int16_t)(200 + k);
    }
    g_ncalled = 0, g_nloads = 0;
    RemoveCar(0);
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

    InitEnvs();
    CHECK(gFreeVis == &gEnvirons[0] && gEnvirons[0].next == &gEnvirons[1] && gEnvirons[99].next == NULL);
    gEnvirons[1].data = 0x1234, gEnvirons[1].func = (void (*)(void *, short))recorder;
    a = GrabEnvEntry(), b = GrabEnvEntry(), c = GrabEnvEntry();
    CHECK(a == &gEnvirons[0] && b == &gEnvirons[1] && c == &gEnvirons[2] && gEnvCount == 3);
    CHECK(b->next == NULL && b->index == 0 && b->objnum == -1 && b->slot == 0 && b->data == 0 && b->func == NULL);
    CHECK(b->timeStamp == 0.0f && gFreeVis == &gEnvirons[3]);
    b->func = (void (*)(void *, short))recorder, b->objnum = 9;
    a->next = b, b->next = c, c->next = NULL, gEnvList = a;
    g_nloads = 0;
    ReleaseEnviron(b, 1);
    CHECK(a->next == c && gFreeVis == b && b->next == &gEnvirons[3] && b->func == NULL && b->objnum == -1);
    CHECK(g_nloads == 1 && g_loads[0] == 9 && gEnvCount == 2);
}

/* Code that reaches cars by raw offsets, strides, or GameCar's padding (m2c). */
static void test_raw(void)
{
    struct car *c = &CARS[1];
    int16_t *table = (int16_t *)(gCheckpoints + 0x30);

    /* SyncMaxpathToLastCheckpoint: car+0x33A = gCheckpoints_pathPts[car+0x33C + model+0x7E2 * 40] */
    CF(c, 0x33C, int16_t) = 3;
    MF(MODEL(1), 0x7E2, int16_t) = 2;
    table[3 + 2 * 40] = 1234;
    SyncMaxpathToLastCheckpoint(1);
    CHECK(CF(c, 0x33A, int16_t) == 1234);
    /* init_view3 (a light flash on the car it gets) */
    CF(c, 0x35C, int8_t) = 4;
    gGameState = 8;
    init_view3(c);
    CHECK(CF(c, 0x35E, int8_t) == 2 && CF(c, 0x35D, int8_t) == 2 && rear_y[4] == 5.0f && view3_switch[4] == 1);
    /* AreWeSolid: a stride on D_80152B70 (car + 0x358) */
    MF(MODEL(1), 0x640, int8_t) = 0, MF(MODEL(1), 0x6C4, int16_t) = -1;
    MF(MODEL(1), 0x7EB, int8_t) = 1, MF(MODEL(1), 0x7DF, int8_t) = 0;
    CF(c, 0x358, int8_t) = 0;
    CHECK(AreWeSolid(1) == 1);
    CF(c, 0x358, int8_t) = 1;
    CHECK(AreWeSolid(1) == 0);
    /* gameover_validOption: a stride on D_80152907 (car + 0xEF) */
    gRace = 3, gCircuitPlayer = 1;
    CF(c, 0xEF, int8_t) = 0;
    CHECK(gameover_validOption(5) == 0);
    CF(c, 0xEF, int8_t) = 1;
    CHECK(gameover_validOption(5) == 1);
    /* CheckCrash: GameCar's pad0EC[0x26C/0x26D] are car + 0x358/0x359 */
    gRace = 0;
    CF(c, 0x358, int8_t) = 0, CF(c, 0x359, int8_t) = 0;
    MF(MODEL(1), 0x640, int8_t) = 1, MF(MODEL(1), 0x714, float) = 7.0f;
    CheckCrash(1);
    CHECK(CF(c, 0x358, int8_t) == 1 && CF(c, 0x10C, float) == 7.0f && CF(c, 0xED, int8_t) == -1);
    MF(MODEL(1), 0x714, float) = 11.0f;
    CheckCrash(1);
    CHECK(CF(c, 0x358, int8_t) == 2);
    /* BlinkResurrectingCars: a car's flash timer (car + 0xEC, 0x309, 0x310, 0x311, 0x30C, 0x359), N64 layout model flags */
    memset(MODEL(0) + 0x7C8, 0, 2);
    MF(MODEL(1), 0x7C8, int16_t) = 1, MF(MODEL(1), 0x7D4, uint32_t) = 0xFF;
    CF(c, 0xEC, int8_t) = 1, CF(c, 0x309, int8_t) = 0, CF(c, 0x310, int8_t) = 0, CF(c, 0x311, int8_t) = 0;
    CF(c, 0x30C, float) = 9.0f, CF(c, 0x359, int8_t) = 0;
    MF(MODEL(1), 0x714, float) = 10.5f;
    BlinkResurrectingCars();
    CHECK(CF(c, 0x311, int8_t) == 1 && CF(c, 0x310, int8_t) == 0 && MF(MODEL(1), 0x7D4, uint32_t) == 0xF7);
    /* InitCPTime (race start): each car's +0x108 cleared and its position (+8) copied out, at the car stride */
    gNumActiveCars = 3;
    for (int i = 0; i < 3; i++) {
        CF(&CARS[i], 0x108, float) = 1.0f;
        CF(&CARS[i], 0x8, float) = 10.0f * (float)i, CF(&CARS[i], 0xC, float) = 1.0f + (float)i;
        CF(&CARS[i], 0x10, float) = -2.0f * (float)i;
    }
    InitCPTime();
    for (int i = 0; i < 3; i++) {
        CHECK(CF(&CARS[i], 0x108, float) == 0.0f);
        CHECK(pc_cp_last_pos[i][0] == 10.0f * (float)i && pc_cp_last_pos[i][1] == 1.0f + (float)i);
        CHECK(pc_cp_last_pos[i][2] == -2.0f * (float)i);
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
    gCarModels[4] = desc;
    memset(m, 0, 0x808);
    MF(m, 8, uint8_t) = 4;               /* car_type */
    MF(m, 11, int8_t) = 2;               /* tune_type */
    TUNES[2 * 11 + 3] = 0.5f;            /* tune +0xC */
    g_ntire = g_nprev = 0;
    carsetup(m);
    CHECK(MF(m, 0, uint32_t) == DESC_AT && MF(m, 4, uint32_t) == W32(&TUNES[2 * 11]));
    CHECK(MF(m, 4, uint32_t) == TEST_N64(gTune) + 2 * 44 && MF(m, 0x40C, uint32_t) == TEST_N64(stdtorquecurve));
    CHECK(g_ntire == 4 && g_nprev == 4);
    for (i = 0; i < 4; i++)
        CHECK(g_tire_info[i] == CURVES_AT + 0x40 * (uint32_t)i);
    CHECK(g_tire_x[0] == -8.0f && g_tire_x[1] == -5.75f && g_tire_x[2] == 2.75f && g_tire_x[3] == 3.75f);
    /* steer_constants reads the descriptor through +0 */
    MF(m, 0x5BC, float) = 4.0f, MF(m, 0x5C8, float) = 2.0f;
    steer_constants(m);
    CHECK(MF(m, 0x3C4, float) == 540.0f / 1080.0f * 2.0f);
    CHECK(MF(m, 0x3C8, float) == 2.0f * 1080.0f / (4.0f * (8.0f / 2.0f)));
    /* find_best_gear: the up-shift gear from tune +0xC and descriptor +0xA4 (both through u32 fields) */
    for (i = 0; i < 6; i++)
        carTransGearRatio[0][i] = 1.0f;
    MF(m, 9, int8_t) = 0, MF(m, 0x41C, float) = 10.0f;    /* 10 * 0.5 * 3 = 15 */
    find_best_gear(m, 14.0f, 0.0f);
    CHECK(MF(m, 0x3F4, int16_t) == 4);
    find_best_gear(m, 16.0f, 0.0f);
    CHECK(MF(m, 0x3F4, int16_t) == 1);
    /* the torque: the curve, scaled by tune +8 (through the u32 at +4) and the table at the model's +0xC, +0xB */
    {
        static int16_t curve[12 * 10];
        for (i = 0; i < 12 * 10; i++)
            curve[i] = 100;
        TUNES[2 * 11 + 2] = 2.0f;        /* tune +8 */
        MF(m, 0xB, int8_t) = 1, MF(m, 0xC, int8_t) = 2;
        carEngineTorque[2][1] = 1.5f;
        CHECK(enginetorque(m, 2000, 50, curve) == 300);
    }
    /* the descriptor's +0xC..+0x14 scale a vector (calcaa), tune +0xC and descriptor +0xA4 the gear force */
    {
        float in[3] = {1.0f, 2.0f, 3.0f}, out[3];
        MF(desc, 0xC, float) = 2.0f, MF(desc, 0x10, float) = 3.0f, MF(desc, 0x14, float) = 4.0f;
        calcaa(m, in, out);
        CHECK(out[0] == 2.0f && out[1] == 6.0f && out[2] == 12.0f);
        MF(m, 0x3F4, int16_t) = 2, MF(m, 9, int8_t) = 1;
        carTransGearRatio_biased[1][2] = 4.0f;
        transmission(m);
        CHECK(MF(m, 0x428, float) == 0.5f * 4.0f && MF(m, 0x42C, float) == 3.0f * 2.0f);
    }
}

/* Update_MDrive (a battle race) moves the other cars' scene nodes by their handles' low halves. */
static void test_race_init(void)
{
    gRace = 2, gNumPlayers = 3;
    CF(&CARS[1], 0xF4, int32_t) = 0x42;
    CF(&CARS[2], 0xF4, int32_t) = 0x43;
    g_nnode = 0;
    Update_MDrive();
    CHECK(g_nnode == 2 && g_node_moved[0] == 0x42 && g_node_moved[1] == 0x43);
    gRace = 0;
}

/* The brake lights (AnimateBrakeLights, AnimateBrakeLights) of a braking car take the brake colours into its palette
 * record (a u32 window address in car_palettes_palBase): the car is the visual's slot (+0xC on the PC; +8 is its index). */
#define PALETTES_AT (TEST_SCRATCH + 0x2000u)
static void test_brake_lights(void)
{
    struct pc_visual v;
    uint16_t *pal = (uint16_t *)(uintptr_t)(PALETTES_AT + 320);
    int i, k;

    for (i = 0; i < 6; i++) {
        CF(&CARS[i], 0xE8, uint32_t) = i == 1 ? 0x1000 : 0;
        MF(car_palettes_palBase, 24 * i, uint32_t) = PALETTES_AT + 0x200 * (uint32_t)i;
        memset((uint8_t *)pal + 0x200 * i, 0, 16);
    }
    for (k = 0; k < 8; k++)
        MF(taillight_colors_dark, 2 * k, uint16_t) = (uint16_t)(0x100 + k), MF(taillight_colors_dim, 2 * k, uint16_t) = (uint16_t)(0x200 + k);
    gTrackNum = 0;
    memset(&v, 0, sizeof v);
    v.index = 3, v.slot = 1;
    AnimateBrakeLights(&v, 1);
    for (i = 0; i < 6; i++)
        for (k = 0; k < 8; k++)
            CHECK(pal[0x100 * i + k] == (i == 1 ? 0x200 + k : 0));
    AnimateBrakeLights(&v, 0);
    CHECK(pal[0x100] == 0x100 && pal[0x107] == 0x107);
    CF(&CARS[1], 0xE8, uint32_t) = 0;
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
