/* port/assets/tests/test_targets.c: the target info table (D_80117530, 0x40 bytes a record on the PC) and the
 * level-object nodes, the targets (0x70 bytes, their N64 layout, in the arena block D_8014D280), through the game's
 * own code (docs/port/POINTERS.md 10): the node pool's set-up (camera_lerp_position, with 32-bit links: the n64
 * flag), the instantiation and per-node set-up (level_objects: transmission_ratio_get, engine_sound_update,
 * engine_torque_calc), the start functions the table names (func_8010DBB8, func_8010E72C, StartCone, func_8010DAF8,
 * func_8010D9CC) and a hit function (func_8010DF90), the target visuals (func_8010E828, func_8010E694,
 * audio_channel_setup, the debris func_8010E4E4, which frees its target into the pool), the collision pass that
 * calls a target's hit function (race_setup_2), the scripted cameras (camera_process_input, func_800C1A00,
 * func_8010C2E4: camera, controller and PTHD scene, all window addresses) and a dead-reckoned collision
 * (func_8010C02C and its response random_float: Matrix64, whose body is a window address). Records past the first are used throughout, so an N64 stride or a host-sized pointer
 * lands somewhere else. The nodes, the heap records and the track records sit in an RDRAM window the test maps
 * (exit 77, skipped, if it can't).
 * game: src/blob/camera_lerp_position.c src/blob/pool_linked_list_init.c src/blob/func_8008E3C0.c
 * game: src/blob/func_800AFA84.c src/blob/groups/level_objects/group.c src/blob/func_800AB750.c
 * game: src/blob/func_8010DBB8.c src/blob/func_8010E72C.c src/blob/StartCone.c src/blob/func_8010DAF8.c
 * game: src/blob/func_8010D9CC.c src/blob/func_8010DF90.c src/blob/func_8010E828.c src/blob/func_8010E694.c
 * game: src/blob/audio_channel_setup.c src/blob/func_8010E4E4.c src/blob/race_setup_2.c
 * game: src/blob/groups/camera_aspect_ratio/group.c src/blob/func_800C1A00.c src/blob/func_8010C2E4.c
 * game: src/blob/func_8010C02C.c src/blob/random_float.c
 */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the PC layouts */

/* A target info record: level_objects' Metadata48 on the PC. */
typedef struct {
    const char *name, *file;
    void (*start)(void *);
    void (*visual)(void *, short);
    int16_t kind;
    uint16_t flags;
    int16_t shape;
    int8_t category, element;
    float value;
    int32_t sound;                   /* N64 +0x1C; the cameras' first track */
    int32_t track;                   /* +0x20: the cameras' moving sound */
    int32_t trackB;                  /* +0x24 */
    int32_t w28;                     /* +0x28 */
    float f2C;                       /* +0x2C */
} TInfo;
_Static_assert(sizeof(TInfo) == TINFO_OFS(0x30) && offsetof(TInfo, sound) == TINFO_OFS(0x1C), "the record");

/* A StatLL header on the PC (shots.c's StatLL) with the n64 flag. */
typedef struct { uint8_t doubly, n64; int32_t count, size; void *mem, *active, *free; } StatLL;

#define NF(n, off, T) (*(T *)((uint8_t *)(n) + (off)))   /* a node field at its N64 offset */
#define W32(p) ((uint32_t)(uintptr_t)(p))
#define WP(a) ((void *)(uintptr_t)(a))
#define ABS(name, addr) __asm__(".globl " #name "\n.set " #name ", " #addr)
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data */

#define NODES 0x8014D280u            /* D_8014D280, 130 nodes of 0x70 bytes in the game arena */
#define NODE(i) ((uint8_t *)WP(NODES + 0x70u * (i)))
ABS(D_8014D280, 0x8014D280);
ABS(D_80118C10, 0x80118C10);         /* the dead-reckoning bodies (arena data) */
ABS(D_8014A250, 0x8014A250);         /* model[6] */
ABS(D_8014A890, 0x8014A890);
#define MODEL(i) ((uint8_t *)WP(0x8014A250u + 0x808u * (i)))
#define HEAP 0x80300000u             /* the game heap (AllocateMemoryHeap), the track's records */

TInfo D_80117530[122];
ALIAS(D_8011754C, D_80117530, 0x2C);
StatLL D_80143FC8, D_80151AA8;
typedef void (*HitFunc)(void *, void *, void *, int32_t);
int32_t (*D_80117518[6])(void *, void *, void *, int32_t);
uint8_t D_80117510[8], D_80150E28[8];
uint8_t *D_80150E68[8], *D_80150E98[8], *D_80118DDC[8];
int32_t D_80150F78, D_80150F80, D_80151688, D_801515F0, D_80151964, D_80151610, D_80151408, D_80150B64;
void *D_80150F38;
intptr_t D_80151528[4][5];
void *D_80151A70;
void *D_801392D0;                    /* Scene36 *: the PTHD scenes, a track WAD record */
int32_t D_801392D4;
void **D_801497C0;
int32_t D_801497EC;
uint32_t D_801174B4, state_word_a;
int8_t D_80156994, D_80150DD0, D_80114650, D_80152570, D_8013FECC, D_8013FECD;
int32_t D_8014A110, gameplay_mode, D_801170FC, D_8013F1DC;
int16_t D_8014A108;
volatile uint8_t gNumModels = 2;
uint16_t D_801427C0[64], D_80142A78, D_80142A7A;
float D_8011418C[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
float D_80118DFC[3], D_80121DDC[3] = {0.0f, -0.25f, 0.0f}, D_801141B0[3];
float D_801249BC = 0.75f, D_801249C0 = 1.25f;
volatile float D_8002EB94 = 0.5f;
int8_t D_8010FFC0;
intptr_t D_8013C300[30];
void *D_80149770;                    /* the collision quadtree (a track record) */
void *D_801497F8;                    /* the road polygons */
void *D_801391F0;
uint8_t D_8012E700[600 * 0x68];      /* the scene nodes (host layout: the model id at +0x18) */
uint8_t D_80152818[6 * PC_CAR_SIZE] __attribute__((aligned(16)));
ALIAS(player_array, D_80152818, 0);
ALIAS(D_80152B74, D_80152818, 0x404);   /* N64 +0x35C */
#define CAR(i) (D_80152818 + PC_CAR_SIZE * (i))

/* ---------------------------------------------------------------- what the code under test calls */

static uint32_t g_heap = HEAP + 0x10000u;
void *AllocateMemoryHeap(int32_t heap, int32_t size)
{
    void *p = WP(g_heap);
    memset(p, 0, (size_t)size);
    g_heap += ((uint32_t)size + 15u) & ~15u;
    return p;
}
static int16_t g_handle = 7;
int32_t func_8008E26C(int32_t key, void *mat, int16_t parent, int32_t flags) { return g_handle++; }
int32_t string_copy_format(const char *name, int8_t a, int8_t b, int8_t c) { return (int32_t)strlen(name) * 16; }
int func_800950AC(const char *a, const char *b, unsigned int n) { return strncmp(a, b, n); }
int n64_strlen(const char *s) { return (int)strlen(s); }
char *func_800A464C(char *s, char *find) { return strstr(s, find); }
void math_utility(float *src, float *dst) { memcpy(dst, src, 9 * sizeof(float)); }
int32_t func_800AB53C(float *pos) { return -1; }
float func_800ABB58(int32_t key) { return 100.0f; }
void transmission_shift(void *xform, int32_t handle) {}
uint32_t func_8008AE64(int16_t id) { return 0; }
void func_8039133C(int32_t a) {}
void listener_position_set(int32_t a) {}
void func_800B2CB4(uint32_t a) {}
void func_800BB7F4(void) {}
void func_803914A8(void) {}
static struct pc_visual g_visuals[16];
static int g_nvisual;
void *func_80090284(void)
{
    struct pc_visual *v = &g_visuals[g_nvisual++ & 15];
    memset(v, 0, sizeof *v);
    return v;
}
static void *g_released[8];
static int g_nreleased;
void entity_transform_apply(void *v, int32_t unlink) { g_released[g_nreleased++ & 7] = v; }
static int16_t g_spawned[8];
static int g_nspawned;
void entity_spawn_callback(int16_t id, int32_t a, int32_t b) { g_spawned[g_nspawned++ & 7] = id; }
static int32_t g_sound;
static void *g_sound_pos;
int32_t stat_lap_split(int32_t sound, int32_t slot, void *pos, uint8_t mode) { g_sound = sound, g_sound_pos = pos; return 0; }
static void *g_rate, *g_uvs;
void sound_position_set(void *rate, void *uvs) { g_rate = rate, g_uvs = uvs; }
void func_80090E9C(float a, void *pos) {}
void vector_normalize_length(void *v, void *m) {}
void save_write_data(void *pos, int32_t a, float s, int32_t b) {}
void model_data_load(int32_t obj, int32_t a, int32_t b) {}
int32_t func_800F7A98(int32_t car, int32_t n) { return 0; }
static int g_interp;
void linear_interp(int32_t car, int32_t n) { g_interp = n; }
void func_8038d3a4(void *a, void *b, int32_t c) {}
void func_800A61B0(float *v, float *out, float *m)
{
    for (int i = 0; i < 3; i++)
        out[i] = m[i * 3] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
}
void func_8009E820(float *in, float *out, float *m)
{
    for (int j = 0; j < 3; j++)
        out[j] = in[0] * m[j] + in[1] * m[3 + j] + in[2] * m[6 + j];
}
static void *g_hit_other;
void random_int(void *car, float *dir) { g_hit_other = car; }   /* random_float's push out of the record's box */
float func_8008B3C8(float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
int8_t D_8017A634;
/* the scripted cameras */
int32_t camera_target_track(float *pos, intptr_t up, float f2C, float a, float b, float c, int32_t id, int32_t d,
                            int32_t s28, int32_t e) { return 40 + id; }
void camera_clip_planes(int32_t h, intptr_t pos, intptr_t up, float a, float b) {}
int32_t leaderboard_update(int32_t h) { return 1; }
void results_screen_update(int32_t h) {}
void func_800BFBE8(float *m, float *rot, int32_t a) {}
void func_800BFD8C(float t, float *a, float *b, float *out) {}
void func_800C0294(int32_t id, float *v) {}
void func_800C0828(float *m) {}
void camera_first_person(int32_t id, float *pos, float *mat, float *m) {}

/* the table's functions */
static void *g_started;
static void start_fn(void *t) { g_started = t; }
static void visual_fn(void *v, short op) {}
static void camera_visual_fn(void *v, short op) {}
static int32_t g_hit_calls;
static int32_t hit_fn(void *a, void *b, void *c, int32_t d) { g_hit_calls++; return 1; }

void camera_lerp_position(int32_t a);
int32_t transmission_ratio_get(void *rec, int16_t parent, float *pos, void *src, int8_t index, int32_t place,
                               int32_t other);
void engine_sound_update(void);
void func_8010DBB8(void *t);
void func_8010E72C(void *t);
void StartCone(void *t);
void func_8010DAF8(void *t);
void func_8010D9CC(void *t);
void func_8010DF90(void *t);
void func_8010E828(void *v, int16_t op);
void func_8010E694(void *v, int16_t op);
void audio_channel_setup(void *v, int16_t op);
void func_8010E4E4(void *v, int16_t op);
void race_setup_2(int16_t player);
void camera_process_input(void *cam);
void func_800C1A00(int32_t id, float *out);
int32_t func_8010C2E4(void *cam, int16_t *player, int32_t a, int32_t b);
int32_t func_8010C02C(void *object, int16_t *index, void *a, int32_t b);
void random_float(int32_t a, void *car, void *other, float *dir, float *pos);

/* ---------------------------------------------------------------- the track: scenes, keys, places */

#define CONE 5                        /* table records: past the first, so an N64 stride misses them */
#define CAMERA 9
#define DEBRIS 12
#define SCENES (HEAP + 0x100u)        /* Scene36[2] */
#define KEYS (HEAP + 0x200u)          /* the camera scene's keys, 0x44 bytes each */
#define PLACE (HEAP + 0x400u)         /* a Place record (the cone's) */
#define STATIC (HEAP + 0x800u)        /* a static per-kind table */

static void setup_table(void)
{
    TInfo *t;

    memset(D_80117530, 0, sizeof D_80117530);
    for (int i = 0; i < 122; i++)
        D_80117530[i].name = "UNUSED", D_80117530[i].file = "none";
    t = &D_80117530[CONE];
    t->name = "CONE", t->file = "cone", t->start = start_fn, t->visual = visual_fn;
    t->kind = 1, t->flags = 0x2 | 0x4 | 0x8 | 0x20, t->shape = 3, t->category = 2, t->value = 3.5f;
    t->sound = 0x77;
    t = &D_80117530[CAMERA];
    t->name = "CAMERA", t->file = "cam", t->start = start_fn, t->visual = camera_visual_fn;
    t->kind = 4, t->flags = 0x8 | 0x20 | 0x8000, t->shape = 361, t->category = 4, t->value = 2.0f;
    t->sound = -1, t->track = -1, t->trackB = -1;
    t = &D_80117530[DEBRIS];
    t->name = "DEBRIS", t->flags = 0x2000;
    D_80117510[2] = 24, D_80117510[4] = 72;   /* the category records: cone 24 bytes, camera CamCtl 72 */
}

static void setup_track(void)
{
    uint8_t *sc = WP(SCENES), *k = WP(KEYS), *place = WP(PLACE);

    memset(sc, 0, 0x400);
    D_801392D0 = WP(SCENES);
    NF(sc + 36, 0x10, uint32_t) = 0x20;       /* scene 1: the camera's; it has three keys */
    NF(sc + 36, 0x14, int16_t) = 3, NF(sc + 36, 0x16, int16_t) = 4;
    NF(sc + 36, 0x1C, uint32_t) = KEYS;
    NF(sc + 36, 0x20, int32_t) = 0x5151;      /* its key */
    for (int i = 0; i < 3; i++) {
        uint8_t *key = k + 0x44 * i;
        NF(key, 0, float) = 100.0f + (float)i, NF(key, 4, float) = 5.0f, NF(key, 8, float) = -(float)i;
        NF(key, 0xC, float) = 1.0f, NF(key, 0x10, float) = 2.0f, NF(key, 0x14, float) = 3.0f + (float)i;
        NF(key, 0x18, float) = 1.0f, NF(key, 0x1C, float) = 1.0f, NF(key, 0x20, float) = 1.0f;
        NF(key, 0x38, float) = 4.0f, NF(key, 0x3C, float) = 10.0f * (float)i;
        NF(key, 0x40, uint32_t) = i == 1 ? 1 : 0;
    }
    memset(place, 0, 0x80);
    strcpy((char *)place, "CONE_A");
    NF(place, 0x10, float) = 1.0f, NF(place, 0x20, float) = 1.0f, NF(place, 0x30, float) = 1.0f;
    NF(place, 0x34, float) = 30.0f, NF(place, 0x38, float) = 0.0f, NF(place, 0x3C, float) = -20.0f;
    NF(place, 0x40, uint32_t) = 0;
    NF(place, 0x4C, int32_t) = 0x4242;        /* key */
    memset(WP(STATIC), 0, 0x100);
    D_80118DDC[2] = WP(STATIC);
}

/* ---------------------------------------------------------------- tests */

/* camera_lerp_position: 130 nodes linked by 32-bit window addresses, their fields after +4 untouched. */
static void test_pool(void)
{
    memset(NODE(0), 0xEE, 130 * 0x70);
    memset(&D_80143FC8, 0, sizeof D_80143FC8);
    D_8014A110 = 0;
    camera_lerp_position(0);
    CHECK(D_80143FC8.n64 == 1 && D_80143FC8.count == 130 && D_80143FC8.size == 0x70);
    CHECK(D_80143FC8.mem == NODE(0) && D_80143FC8.free == NODE(0) && D_80143FC8.active == NULL);
    CHECK(NF(NODE(0), 0, uint32_t) == W32(NODE(1)) && NF(NODE(1), 0, uint32_t) == W32(NODE(2)));
    CHECK(NF(NODE(128), 0, uint32_t) == W32(NODE(129)) && NF(NODE(129), 0, uint32_t) == 0);
    CHECK(NF(NODE(0), 4, uint32_t) == 0xEEEEEEEEu && NF(NODE(1), 0x6C, uint32_t) == 0xEEEEEEEEu);
    memset(NODE(0), 0, 130 * 0x70);
    camera_lerp_position(0);
}

/* Instantiate a cone (with its place record) and a camera (scene 1), then set them up. */
static void test_instantiate(void)
{
    uint8_t *cone, *cam, *ctl;
    struct pc_visual *v;
    float zero[3] = {0, 0, 0};
    StatLL *pool = &D_80143FC8;

    D_80156994 = 1, D_8014A108 = 1, D_801174B4 = 0, D_801392D4 = 0, D_801497EC = 0, g_nvisual = 0, g_started = NULL;
    D_801427C0[3] = 0x1234;
    CHECK(transmission_ratio_get(WP(PLACE), -1, zero, WP(PLACE), 0, 1, 1) == 1);
    strcpy((char *)WP(PLACE + 0x100), "NOTHING");
    CHECK(transmission_ratio_get(WP(PLACE + 0x100), -1, zero, NULL, 1, 1, 1) == 0);   /* no such kind */
    strcpy((char *)WP(PLACE + 0x100), "CAMERA_1");
    CHECK(transmission_ratio_get(WP(PLACE + 0x100), -1, zero, NULL, 1, 1, 1) == 1);
    cone = NODE(0), cam = NODE(1);
    CHECK(pool->active == cam && NF(cam, 0, uint32_t) == W32(cone) && NF(cone, 0, uint32_t) == 0);
    CHECK(pool->free == NODE(2) && NF(NODE(2), 0, uint32_t) == W32(NODE(3)));
    CHECK(NF(cone, 0x10, int16_t) == CONE && NF(cam, 0x10, int16_t) == CAMERA && NF(cone, 0x65, int8_t) == 0);
    CHECK(NF(cone, 0x54, float) == 3.5f && NF(cone, 0x58, int16_t) == 3 && NF(cone, 0x0C, int32_t) == 7);
    CHECK(NF(cone, 0x08, int32_t) == 0x4242 && NF(cam, 0x08, int32_t) == 0x5151 && (NF(cam, 4, uint8_t) & 8));
    CHECK(NF(cone, 0x38, float) == 30.0f && NF(cone, 0x40, float) == -20.0f);
    CHECK(D_80150E28[2] == 1 && D_80150E28[4] == 1 && D_80150F78 == 1);

    engine_sound_update();
    ctl = NF(cam, 0x6C, uint8_t *) ? WP(NF(cam, 0x6C, uint32_t)) : NULL;
    CHECK(W32(D_80150E68[2]) >= HEAP && NF(cone, 0x6C, uint32_t) == W32(D_80150E68[2]));
    CHECK(D_80150E98[2] == D_80150E68[2] + 24 && ctl == D_80150E68[4]);
    CHECK(ctl && NF(ctl, 0, uint32_t) == SCENES + 36);              /* CamCtl.scene: &D_801392D0[1] */
    CHECK(W32(D_80150F38) >= HEAP && NF(cam, 0x65, int8_t) == 0 && D_80150F80 == 1);
    CHECK(NF(D_80150F38, 0, uint32_t) == 0x80118C10u + 2 * 32);    /* its body record */
    CHECK(NF(D_80150F38, 4, float) == 1.0f && NF(D_80150F38, 0x14, float) == 1.0f);
    CHECK(NF(D_8012E700 + 7 * 0x68, 0x18, uint16_t) == 0x1234);    /* the cone's texture (N64 +0x14) */
    v = (struct pc_visual *)D_801391F0;                              /* the cone's animation */
    CHECK(v == &g_visuals[0] && v->data == W32(cone) && v->func == visual_fn && v->timeStamp == 0.0f);
    CHECK(g_started == cone);                                        /* flags & 4: its start function */
}

/* The start functions: a visual with the table's visual function and the target, and the table's sound. */
static void test_start(void)
{
    uint8_t *t = NODE(0), *car = CAR(1);
    struct pc_visual *v;
    uint8_t *motion = WP(HEAP + 0x900u);

    memset(car, 0, PC_CAR_SIZE);
    NF(car, 0x14, float) = 8.0f, NF(car, 0x18, float) = 16.0f, NF(car, 0x1C, float) = 24.0f;
    NF(t, 0x5C, int8_t) = 1;
    NF(t, 0x2C, float) = 1.0f;

    D_801391F0 = NULL, g_nvisual = 0, g_sound = 0;
    func_8010DBB8(t);
    v = &g_visuals[0];
    CHECK(D_801391F0 == v && v->func == visual_fn && v->data == W32(t) && v->timeStamp == 0.0333333f);
    CHECK(NF(t, 0x5A, int16_t) == 7 && g_sound == 0x77 && g_sound_pos == t + 0x38 && !(NF(t, 4, uint8_t) & 6));

    g_sound = 0;
    func_8010E72C(t);
    v = &g_visuals[1];
    CHECK(D_801391F0 == v && v->next == &g_visuals[0] && v->func == visual_fn && v->data == W32(t));
    CHECK(NF(t, 0x5A, int16_t) == 4 && g_sound == 0x77);

    g_sound = 0;
    func_8010DAF8(t);
    v = &g_visuals[2];
    CHECK(D_801391F0 == v && v->func == visual_fn && v->data == W32(t) && v->timeStamp == 1.25f && v->index == 0);
    CHECK(NF(t, 0x5A, int16_t) == 0xC && g_sound == 0x77);

    g_sound = 0;
    func_8010D9CC(t);
    v = &g_visuals[3];
    CHECK(D_801391F0 == v && v->func == visual_fn && v->data == W32(t) && v->timeStamp == 0.75f);
    CHECK(NF(t, 0x5A, int16_t) == 7 && g_sound == 0x77);

    memset(motion, 0, 24);
    NF(t, 0x6C, uint32_t) = W32(motion);
    D_80117530[CONE].shape = 3;
    g_sound = 0;
    StartCone(t);
    v = &g_visuals[4];
    CHECK(D_801391F0 == v && v->func == visual_fn && v->data == W32(t) && v->timeStamp == 5.0f);
    CHECK(NF(motion, 0xC, float) == 1.0f && NF(motion, 0x10, float) == 4.0f && NF(motion, 0x14, float) == 3.0f);
    CHECK(NF(motion, 4, float) == 12.0f && NF(motion, 8, float) == 15.0f && g_sound == 0x77);
}

/* A hit function: its state is the target's +0x6C, a window address. */
static void test_hit(void)
{
    uint8_t *t = NODE(0), *state = WP(HEAP + 0xA00u);

    memset(state, 0, 0x50);
    NF(state, 0x4A, uint16_t) = 9, NF(state, 0x48, uint16_t) = 1;   /* car 0's slot bit */
    NF(t, 0x6C, uint32_t) = W32(state);
    NF(t, 0x5C, int8_t) = 0;
    NF(CAR(0), CAR_OFS(0x35C), int8_t) = 0;
    g_interp = 0, g_sound = 0;
    func_8010DF90(t);
    CHECK(g_interp == 9 && NF(state, 0x48, uint16_t) == 0 && (NF(t, 4, uint8_t) & 0x80) && g_sound == 0x77);
}

/* The target visuals: each finds its target through the visual's 32-bit data, and the target's state. */
static void test_visuals(void)
{
    uint8_t *t = NODE(2), *rate = WP(HEAP + 0xB00u);
    struct pc_visual v;

    memset(t, 0, 0x70);
    memset(&v, 0, sizeof v);
    v.data = W32(t);
    NF(t, 0x6C, uint32_t) = W32(rate);
    NF(rate, 0, float) = 2.0f, NF(rate, 4, float) = 4.0f, NF(rate, 8, float) = 6.0f;

    NF(t, 0x5A, int16_t) = 2, g_nreleased = 0;
    func_8010E828(&v, 1);
    CHECK(g_rate == rate && g_uvs == t + 0x14 && NF(t, 0x5A, int16_t) == 1 && g_nreleased == 0);
    func_8010E828(&v, 1);
    CHECK(NF(t, 0x5A, int16_t) == 0 && g_nreleased == 1 && g_released[0] == &v);

    g_uvs = NULL;
    func_8010E694(&v, 1);
    CHECK(g_uvs == t + 0x14 && g_rate != rate);   /* rate * frame time, on the stack */
    CHECK(((float *)g_rate)[0] == 1.0f && ((float *)g_rate)[2] == 3.0f);

    /* the frame stepper: the last frame of a kind with flag 0x2000 removes the object */
    NF(t, 0x10, int16_t) = DEBRIS, NF(t, 0x0C, int32_t) = 33;   /* the handle; its low half is the model id */
    NF(t, 0x58, int16_t) = 4, NF(t, 0x5A, int16_t) = 2, NF(t, 0x50, int16_t) = 4;
    v.index = 1, v.timeStamp = 0.0f, g_nspawned = 0, g_nreleased = 0;
    audio_channel_setup(&v, 1);
    CHECK(g_nspawned == 1 && g_spawned[0] == 33 && g_nreleased == 1 && g_released[0] == &v);
}

/* The debris: its physics state moves it, and when its time is up it goes back to the pool. */
static void test_debris(void)
{
    StatLL *pool = &D_80143FC8;
    uint8_t *t = NODE(1), *phys = WP(HEAP + 0xC00u);
    struct pc_visual v;
    void *free_before = pool->free;

    memset(phys, 0, 24);
    NF(t, 0x6C, uint32_t) = W32(phys);
    NF(t, 0x10, int16_t) = DEBRIS, NF(t, 0x0C, int32_t) = 44, NF(t, 0x38, float) = 1.0f;
    memset(&v, 0, sizeof v);
    v.data = W32(t), v.timeStamp = 1.0f;
    g_nspawned = 0, g_nreleased = 0;
    func_8010E4E4(&v, 1);
    CHECK(NF(phys, 0x10, float) == -0.25f * 0.15f && NF(t, 0x3C, float) == 2 * (-0.25f * 0.15f));
    CHECK(v.timeStamp == 0.5f && g_nreleased == 0 && pool->active == t);
    func_8010E4E4(&v, 1);
    CHECK(g_nspawned == 1 && g_spawned[0] == 44 && g_nreleased == 1 && g_released[0] == &v);
    CHECK(pool->free == t && NF(t, 0, uint32_t) == W32(free_before) && pool->active == NODE(0));
}

/* The collision pass: the target in the car's quadtree leaf is hit through the table's kind (its shape: +0x10)
 * and hit function. */
static void test_collision(void)
{
    uint8_t *tree = WP(HEAP + 0xD00u), *t = NODE(0);
    static void *side[4];

    memset(tree, 0, 28);
    NF(tree, 3, uint8_t) = 0;                         /* a leaf */
    NF(tree, 4, float) = -1000.0f, NF(tree, 8, float) = 1000.0f, NF(tree, 12, float) = -1000.0f;
    NF(tree, 16, float) = 1000.0f, NF(tree, 20, uint16_t) = 1, NF(tree, 22, uint16_t) = 2;
    D_80149770 = tree;
    side[2] = t;
    D_801497C0 = side;
    NF(t, 4, uint8_t) = 2 | 8;
    for (int i = 0; i < 6; i++)
        D_80117518[i] = NULL;
    D_80117518[1] = hit_fn;                           /* the cone's kind */
    D_8014A110 = 0, D_801174B4 = 0, g_hit_calls = 0, g_started = NULL;
    race_setup_2(1);
    CHECK(g_hit_calls == 1 && g_started == t && (NF(t, 4, uint8_t) & 4) && NF(t, 0x5C, int8_t) == 1);
}

/* A scripted camera: its controller (a heap record) and its scene (a PTHD record) through 32-bit fields. */
static void test_camera(void)
{
    uint8_t *cam = NODE(1), *ctl = D_80150E68[4], *sc = WP(SCENES + 36), *k = WP(KEYS);
    struct pc_visual *v;
    float out[3];
    int16_t player = 0;
    uint8_t *polys = WP(HEAP + 0xE00u);

    /* the debris test freed node 1; give the camera back its fields */
    NF(cam, 0x10, int16_t) = CAMERA, NF(cam, 0x6C, uint32_t) = W32(ctl), NF(cam, 4, uint8_t) = 0;
    D_8013F1DC = 0, gameplay_mode = 0, D_801391F0 = NULL, g_nvisual = 0;
    camera_process_input(cam);
    v = &g_visuals[0];
    CHECK(D_8013F1DC == 1 && D_8013C300[0] == (intptr_t)cam && NF(cam, 0x64, int8_t) == 2 && (NF(cam, 4, uint8_t) & 1));
    CHECK(NF(ctl, 0x14, float) == 20.0f && NF(ctl, 0xC, int16_t) == 1 && NF(ctl, 0xE, uint16_t) == 4);
    CHECK(NF(cam, 0x38, float) == 101.0f && NF(cam, 0x3C, float) == 5.0f && NF(ctl, 0x18, float) == 101.0f);
    CHECK((NF(k + 0x44, 0x40, uint32_t) & 0x1000) && NF(cam, 0x5A, int16_t) == 4);
    CHECK(D_801391F0 == v && v->func == camera_visual_fn && v->data == W32(cam) && v->timeStamp == 0.25f);

    NF(ctl, 0x10, float) = 2.0f;
    func_800C1A00(4, out);
    CHECK(out[0] == 2.0f && out[1] == 4.0f && out[2] == 8.0f);    /* key 1's direction * f10 */

    memset(polys, 0, 4 * 24);
    NF(polys + 24, 2, uint16_t) = 0x20 | (4 << 11);               /* a camera trigger of scene 4 */
    D_801497F8 = polys;
    memset(MODEL(0), 0, 0x808);
    NF(MODEL(0), 0x6C4, int16_t) = -1;
    NF(MODEL(0), 0x5A0, uint16_t) = 1;
    func_8010C2E4(cam, &player, 0, 0);
    CHECK((NF(sc, 0x10, uint32_t) & (1u << 24)) && (NF(sc, 0x10, uint32_t) & 0x200));
}

/* A dead-reckoned collision: the record's body is a 32-bit window address, read by the hit test (func_8010C02C)
 * and by the response (random_float), which reads the record's basis, position and velocity after it. */
static void test_reckon(void)
{
    uint8_t *obj = NODE(1), *body = WP(0x80118C10u + 2 * 32), *rec = D_80150F38, *m = MODEL(0);
    int16_t index = 0;
    float dir[3] = {0, 0, 0}, pos[3] = {0, 0, 0};

    memset(body, 0, 32);
    NF(body, 0, float) = -10.0f, NF(body, 4, float) = 10.0f, NF(body, 8, float) = 10.0f;
    NF(body, 12, float) = -10.0f, NF(body, 16, float) = 10.0f, NF(body, 20, float) = -10.0f;
    NF(body, 24, float) = 5.0f, NF(body, 28, float) = 4.0f;   /* radius, mass */
    memcpy(rec + 4, D_8011418C, sizeof D_8011418C);
    memset(rec + 0x28, 0, 24);
    NF(obj, 0x65, int8_t) = 0;
    memset(m, 0, 0x808);
    NF(m, 0x7EA, int8_t) = 1, NF(m, 0x654, float) = 3.0f, NF(m, 0x640, int8_t) = 1;
    memcpy(m + 0x798, D_8011418C, sizeof D_8011418C);
    memcpy(m + 0x7A0, D_8011418C, sizeof D_8011418C);
    /* The car is inside the record's box and already pushed (+0x640): random_float hands it to random_int. */
    g_hit_other = NULL;
    func_8010C02C(obj, &index, NULL, 0);
    CHECK(g_hit_other == m);
    /* Past the box's far side (z 12 > 10) and closing at 1 along z: a push of 2500 x the mass ratio (4 / 2). */
    NF(m, 0x79C, float) = 12.0f;
    NF(rec, 0x3C, float) = -1.0f;                              /* the record's velocity z */
    g_hit_other = NULL;
    random_float(0, m, rec, dir, pos);
    CHECK(g_hit_other == NULL);
    CHECK(NF(m, 0x124, float) == 0.0f && NF(m, 0x128, float) == 0.0f && NF(m, 0x12C, float) == -5000.0f);
}

int main(void)
{
    if (!test_rdram()) {
        fprintf(stderr, "test_targets: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    pc_fix_absolute_got();
    setup_table();
    setup_track();
    test_pool();
    test_instantiate();
    test_start();
    test_hit();
    test_visuals();
    test_debris();
    test_collision();
    test_camera();
    test_reckon();
    return TEST_DONE();
}
