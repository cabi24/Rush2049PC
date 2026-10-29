/* port/assets/tests/test_fxcars.c: the effects' per-car arrays (pc_tire_fx, pc_smoke_shade, pc_trail_pts, pc_blasts,
 * pc_blast_rings, pc_skids, pc_resurrect_mid: port/include/pc_game_types.h, docs/port/DECOUPLE.md 6) through the
 * game's own code for a race of 12 cars, slots 0 to 11, with more slots declared than that. This test and its game
 * objects are built with PC_MAX_LINKS 16 (CMakeLists.txt), so each array has 16 entries where the game build has 12
 * and the N64 6. Every entry starts as a sentinel: the resets must reach all 16, and each car's code its own slot only.
 * physics_collision_test sets up the puff pool and clears the tire-effect latches; camera_smooth_lerp (InitBlasts)
 * takes its 50 blast matrices and marks every blast and ring empty; entity_spawn_init and func_8008E408 start each
 * car's tire puffs (a latch bit, two puffs from the pool) and hud_render ends them (the latch cleared);
 * entity_spawn_init's smoke sets and fades a car's grey, its trails keep their last points; func_80092484 and
 * func_800CB718 clear a car's trail and smoke latches; func_80090308 (StartNewBlast), save_write_data (StartBlast,
 * with StartNewBlastRing), entity_update_callback (AnimateNewBlast) and entity_collision_detect (the ring) run car
 * 11's blast and ring, whose matrices the scene nodes point at; func_800B0580 clears the skid table and cpak_init
 * starts, continues and stops the skids of 12 cars x 4 tires; track_select_handler sets car 11's resurrect midpoint.
 * game: src/blob/physics_collision_test.c src/blob/camera_smooth_lerp.c
 * game: src/blob/groups/entity_spawn_init/entity_spawn_init.c src/blob/groups/func_8008E408/func_8008E408.c
 * game: src/blob/hud_render.c src/blob/func_80092484.c src/blob/func_800CB718.c src/blob/groups/blast/blast.c
 * game: src/blob/entity_update_callback.c src/blob/entity_collision_detect.c src/blob/func_800B0580.c
 * game: src/blob/groups/skid_marks/group.c src/blob/func_800AF844.c src/blob/track_select_handler.c
 * game: src/blob/pool_linked_list_init.c src/blob/func_8008E3C0.c src/blob/func_800AFA84.c
 * game: src/blob/func_8008B2E4.c src/blob/func_8008B2B4.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
#define CARS 12              /* the race's cars: slots 0 to 11 */
#define SENTINEL 0xEE

/* ---------------------------------------------------------------- the storage under test (data.c's in the game) */

unsigned int pc_tire_fx[PC_MAX_LINKS];
unsigned char pc_smoke_shade[PC_MAX_LINKS];
float pc_trail_pts[PC_MAX_LINKS][6];
float pc_resurrect_mid[PC_MAX_LINKS][3];
struct pc_blast pc_blasts[PC_MAX_LINKS];
struct pc_blast_ring pc_blast_rings[PC_MAX_LINKS];
struct pc_skid pc_skids[PC_MAX_LINKS][4];

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_car pc_cars[PC_CAR_SLOTS];
struct pc_model pc_models[PC_MODEL_SLOTS];
struct pc_link pc_links[PC_LINK_SLOTS];
#define CF(i, n, T) (*(T *)((uint8_t *)&pc_cars[i] + CAR_OFS(n)))   /* car i's field at N64 offset n */
#define MF(i, off, T) (*(T *)((uint8_t *)&pc_models[i] + (off)))     /* model i's field */

/* A StatLL list header, as every view has it on the PC (40 bytes): the element array, the active and free lists. */
typedef struct { int8_t dbl; int32_t count, size; void *list, *head, *store; } StatLL;
typedef struct Puff {   /* hud_render's Puff: 88 bytes on the N64, 96 here */
    struct Puff *next;
    float matrix[3][3], position[3];
    int32_t id;
    uint8_t pad56[4];
    float life, timer, velocity[3];
    uint32_t flags;
    int16_t car, frame;
} Puff;
typedef struct EffectObj {   /* skid_marks' skid segment */
    void *next, *prev;
    float f8;
    void *dl;
    float pos[3];
    uint8_t color[4];
    int32_t f20;
} EffectObj;
StatLL D_8013F1E0 __attribute__((aligned(16)));   /* the puff pool */
ALIAS(D_8013F1F0, D_8013F1E0, 24);                /* its active list (N64 +0x10) */
Puff D_8013C378[100];
StatLL D_80155220;                                /* the skid segment pool */
EffectObj D_80155B30[100];
uint8_t D_8012E700[64 * 104] __attribute__((aligned(16)));   /* the scene nodes, 104 bytes each on the PC */

int8_t D_80156994 = 1, D_8014978C, D_80143F14 = 1, D_801427A1, D_8010FFC0;
int16_t D_8014A108 = 1, active_player_count = 1, D_8013C094, D_80142946, D_80142958;
int32_t D_801170FC, D_8011735C = 1, D_80117498, state_word_a;
uint8_t gNumModels = 1;
volatile float D_8002EB94 = 1.0f / 60;
float D_801543CC, D_80154390 = 1.0f, D_8011744C[13], D_8011418C[4][3], D_80123C08 = 0.8f, D_80123C0C = 0.1f;
float D_8002EB90[4], D_8011AD90[16];
uint16_t D_8013F380[6], D_80142948[2][9], D_8014295A[8], D_801427C0[200], D_80161378;
char *D_80117480[6];
int8_t D_80111299[PC_MAX_LINKS * 13];   /* the four players' option per car type in the game: [4][13] */
int32_t D_8011749C, D_801174A0, D_801174A4, D_801174A8, D_801174AC, D_801174B0;
struct { uint8_t *data; uint32_t other; } gTexDefPtrs[64];
void *D_8013C238[50];
void *D_801391F0;
uint32_t D_8011743C[4] = { 1, 2, 4, 8 };
uint8_t D_80117438[4] = { 1, 2, 3, 4 }, D_8011AD8C[4] = { 5, 6, 7, 8 };
struct { uint8_t pad[2]; uint16_t flags; uint8_t pad4[0x14]; } g_surface[4];
void *D_801497F8 = g_surface;
struct { float uvs[3][3], pos[3]; } gCamera;

/* ---------------------------------------------------------------- what the code under test calls */

static float g_mats[50][12];
static int g_nmats;
void *func_800A7D6C(void) { return g_mats[g_nmats++ % 50]; }   /* GetMatrix */

static struct { int32_t obj; const void *mat; } g_obj[256];
static int g_nobj;
int32_t func_8008E26C(int32_t obj, void *mat, int16_t parent, int32_t flags)   /* ZOID_AddObject: a scene node */
{
    (void)parent; (void)flags;
    if (g_nobj < 256) g_obj[g_nobj].obj = obj, g_obj[g_nobj].mat = mat;
    return 1 + g_nobj++ % 60;
}
static struct {   /* a visual (tVisual), 0x20 bytes on the PC */
    void *next;
    int16_t index, objnum, slot;
    uint8_t pad[2];
    uint32_t data;
    float timeStamp;
    void *func;
} g_vis[16];
static int g_nvis;
void *func_80090284(void) { return g_nvis < 16 ? &g_vis[g_nvis++] : NULL; }   /* GrabEnvEntry */
static int g_nremoved;
void entity_spawn_callback(int16_t obj, int32_t a, int32_t b) { (void)obj; (void)a; (void)b; g_nremoved++; }
void entity_transform_apply(void *v, int32_t unlink) { (void)v; (void)unlink; }
void math_utility(void *src, void *dst) { (void)src; (void)dst; }
void vector_normalize_length(void *v, void *m) { (void)v; (void)m; }
void vector_copy_scale(float *in, float *out) { out[0] = in[0], out[1] = in[1], out[2] = in[2]; }
float func_8008E0B8(float *v) { (void)v; return 1.0f; }
float func_8008B3C8(float *v) { (void)v; return 80.0f; }
void model_data_load(int32_t h, int32_t m, uint32_t v) { (void)h; (void)m; (void)v; }
void model_transform_setup(int32_t h, int32_t m, uint32_t v) { (void)h; (void)m; (void)v; }
void func_8008D870(int16_t h, void *r, int32_t m) { (void)h; (void)r; (void)m; }
void func_800B24EC(char *a, int16_t *b, int c, int d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
void func_8038A408(float *pos, int16_t slot) { (void)pos; (void)slot; }
void func_80090770(int16_t obj, uint16_t def) { (void)obj; (void)def; }
void func_80090B68(void *v, int16_t op) { (void)v; (void)op; }
int camera_collision_check(const float *p, intptr_t d, float a, float b, float c, float e, int f, int g, int h,
                           unsigned char i) { (void)p; (void)d; (void)a; (void)b; (void)c; (void)e; (void)f; (void)g;
                           (void)h; (void)i; return 0; }
void PitchUV(float a, float uv[3][3]) { (void)a; (void)uv; }
void func_80090E9C(float a, float uv[3][3]) { (void)a; (void)uv; }
static int g_ndl, g_nfreed_dl;
static int g_dls[128];
void *func_800A78BC(int32_t n, float *a, uint16_t b, uint8_t *c, int32_t d, int32_t e)
{
    (void)n; (void)a; (void)b; (void)c; (void)d; (void)e;
    return &g_dls[g_ndl++ % 128];
}
void func_8008D0C0(void *dl) { (void)dl; g_nfreed_dl++; }
void func_8008C074(void *dl, int32_t n, float *quad, int32_t a, uint8_t *c, int32_t b, int32_t d)
{ (void)dl; (void)n; (void)quad; (void)a; (void)c; (void)b; (void)d; }
void func_800CFDEC(float *a, float *b, int16_t n, float lo, float hi, float t, float *out)
{
    (void)a; (void)b; (void)lo; (void)hi; (void)t;
    for (int k = 0; k < n; k++) out[k] = 100.0f + k;
}
void menu_vibration_test(float *quat, float uvs[3][3]) { (void)quat; (void)uvs; }
void menu_video_settings(void *m) { (void)m; }
void *n64_memset(void *p, int c, uint32_t n) { return memset(p, c, n); }

void physics_collision_test(void);
void camera_smooth_lerp(void);
void entity_spawn_init(int16_t slot, uint32_t type, int32_t fast, float *offset);
void hud_render(void);
void func_80092484(int16_t slot, int16_t which);
void func_800CB718(int16_t slot);
void func_80090308(int16_t slot);
void save_write_data(void *data, int linkToCar, float scale, int playSound);
void entity_update_callback(void *v, int16_t op);
void entity_collision_detect(void *node, int16_t update);
void func_800B0580(void);
void cpak_init(int32_t slot);
void track_select_handler(void *m);

/* ---------------------------------------------------------------- helpers */

/* Every byte of entries [from, to) of an array of n-byte entries is the sentinel. */
static int sentinel(const void *a, size_t n, int from, int to)
{
    const uint8_t *p = a;
    for (size_t k = (size_t)from * n; k < (size_t)to * n; k++)
        if (p[k] != SENTINEL)
            return 0;
    return 1;
}
#define UNTOUCHED(arr, from, to) sentinel(arr, sizeof (arr)[0], from, to)

static int count_list(void *head)
{
    int n = 0;
    for (void **p = head; p; p = *p)
        n++;
    return n;
}

int main(void)
{
    int s, t, k;

    static uint8_t texdefs[64 * 36];
    gTexDefPtrs[0].data = texdefs;
    memset(pc_cars, 0, sizeof pc_cars);
    memset(pc_models, SENTINEL, sizeof pc_models);
    memset(pc_tire_fx, SENTINEL, sizeof pc_tire_fx);
    memset(pc_smoke_shade, SENTINEL, sizeof pc_smoke_shade);
    memset(pc_trail_pts, SENTINEL, sizeof pc_trail_pts);
    memset(pc_resurrect_mid, SENTINEL, sizeof pc_resurrect_mid);
    memset(pc_blasts, SENTINEL, sizeof pc_blasts);
    memset(pc_blast_rings, SENTINEL, sizeof pc_blast_rings);
    memset(pc_skids, SENTINEL, sizeof pc_skids);
    for (s = 0; s < PC_CAR_SLOTS; s++) {
        CF(s, 0xF8, int16_t) = 100 * 4;                 /* 100 mph: the tire puffs need 10 */
        CF(s, 0x35C, int8_t) = 0;                       /* the first window's */
        MF(s, 8, uint8_t) = 0;                          /* car type 0 */
        for (k = 0; k < 3; k++)
            PC_CAR_POS(s)[k] = 1000.0f * s + k;
        for (t = 0; t < 4; t++)
            for (k = 0; k < 3; k++)
                PC_CAR_WHEEL_POS(s)[t][k] = 1000.0f * s + 10.0f * t + k;
    }

    /* The race's start: the puff pool set up, every slot's latches cleared, not the N64's 6. */
    physics_collision_test();
    CHECK(D_8013F1E0.list == D_8013C378 && D_8013F1E0.count == 100 && D_8013F1E0.size == (int32_t)sizeof(Puff));
    for (s = 0; s < PC_MAX_LINKS; s++)
        CHECK(pc_tire_fx[s] == 0);

    /* InitBlasts: 50 matrices from GetMatrix, whatever the cars, and every blast and ring marked empty, the rest of
     * their bytes left alone. */
    g_nmats = 0;
    camera_smooth_lerp();
    CHECK(g_nmats == 50 && D_8013C238[49] == g_mats[49] && D_8013C094 == 0);
    for (s = 0; s < PC_MAX_LINKS; s++) {
        CHECK(pc_blast_rings[s].ringIdx == -1 && sentinel(&pc_blast_rings[s].mat, sizeof pc_blast_rings[s].mat, 0, 1));
        CHECK(pc_blasts[s].glowIdx == -1 && sentinel(&pc_blasts[s].mat, sizeof pc_blasts[s].mat, 0, 1));
        for (k = 0; k < 4; k++)
            CHECK(pc_blasts[s].blastObjs[k].mbIdx == -1 && pc_blasts[s].blastObjs[k].ht != 0.0f);
    }

    /* Each car's front-left tire puffs: its latch bit 0x100 and two puffs from the pool, for its own slot. */
    for (s = 0; s < CARS; s++)
        entity_spawn_init(s, 0, 1, NULL);
    for (s = 0; s < CARS; s++)
        CHECK(pc_tire_fx[s] == 0x100);
    for (s = CARS; s < PC_MAX_LINKS; s++)
        CHECK(pc_tire_fx[s] == 0);
    CHECK(count_list(D_8013F1E0.head) == 2 * CARS);
    entity_spawn_init(CARS - 1, 0, 1, NULL);            /* latched: no more puffs */
    CHECK(count_list(D_8013F1E0.head) == 2 * CARS);
    {
        int cars_seen[CARS] = { 0 };
        for (Puff *p = D_8013F1E0.head; p; p = p->next)
            if (p->car >= 0 && p->car < CARS && p->flags == 0x10)
                cars_seen[p->car]++;
        for (s = 0; s < CARS; s++)
            CHECK(cars_seen[s] == 2);
    }
    /* hud_render ends them (the tires' visual code isn't a puff's): every car's latch bit cleared, the puffs freed. */
    hud_render();
    for (s = 0; s < PC_MAX_LINKS; s++)
        CHECK(pc_tire_fx[s] == 0);
    CHECK(D_8013F1E0.head == NULL);

    /* Car 11's smoke: the latches 0x40, 0x20 and 0x10, its grey set to 175 and faded by 3 a puff; then two frames
     * latched (0x40 to 0x80, 0x80 off), then another puff, faded again. */
    entity_spawn_init(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x40 | 0x20 | 0x10) && pc_smoke_shade[CARS - 1] == 172);
    entity_spawn_init(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x80 | 0x20 | 0x10));
    entity_spawn_init(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x20 | 0x10));
    entity_spawn_init(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x40 | 0x20) && pc_smoke_shade[CARS - 1] == 169);
    CHECK(UNTOUCHED(pc_smoke_shade, 0, CARS - 1) && UNTOUCHED(pc_smoke_shade, CARS, PC_MAX_LINKS));
    for (s = 0; s < CARS - 1; s++)
        CHECK(pc_tire_fx[s] == 0);
    func_800CB718(CARS - 1);                            /* the smoke's 0x20 */
    CHECK(pc_tire_fx[CARS - 1] == 0x40);

    /* Car 11's two trails: their first points kept (0x4000, 0x8000), each in its half of the car's six floats.
     * (The game starts the trails for the one human only, buffer_swap; the slot here shows the storage's.) */
    {
        float offset[3] = { 0.0f, 0.0f, 0.0f };
        D_8014A108 = 1;
        entity_spawn_init(CARS - 1, 6, 0, offset);
        CHECK((pc_tire_fx[CARS - 1] & 0x4000) && !(pc_tire_fx[CARS - 1] & 0x8000));
        CHECK(pc_trail_pts[CARS - 1][0] != 0.0f && UNTOUCHED(pc_trail_pts[CARS - 1], 3, 6));
        entity_spawn_init(CARS - 1, 7, 0, offset);
        CHECK(pc_tire_fx[CARS - 1] & 0x8000);
        CHECK(!sentinel(pc_trail_pts[CARS - 1], sizeof(float), 3, 6));
        CHECK(UNTOUCHED(pc_trail_pts, 0, CARS - 1) && UNTOUCHED(pc_trail_pts, CARS, PC_MAX_LINKS));
        func_80092484(CARS - 1, 0);                     /* the left trail's 0x4000 */
        func_80092484(CARS - 1, 1);                     /* the right's 0x8000 */
        CHECK(!(pc_tire_fx[CARS - 1] & 0xC000));
    }

    /* Car 11's blast: four blast objects and the glow, each a scene node on its matrix in the car's record. */
    {
        int first = g_nobj;
        func_80090308(CARS - 1);
        for (k = 0; k < 4; k++) {
            CHECK(pc_blasts[CARS - 1].blastObjs[k].mbIdx != -1 && pc_blasts[CARS - 1].blastObjs[k].scale == 0.4f);
            CHECK(g_obj[first + k].mat == &pc_blasts[CARS - 1].blastObjs[k].mat);
            CHECK(pc_blasts[CARS - 1].blastObjs[k].mat.pos[0] == PC_CAR_POS(CARS - 1)[0]);
        }
        CHECK(pc_blasts[CARS - 1].glowIdx != -1 && g_obj[first + 4].mat == &pc_blasts[CARS - 1].mat);
        CHECK(pc_blasts[CARS - 1].alpha == 95 && pc_blasts[CARS - 1].lifeTime == 0.7f);
        for (s = 0; s < PC_MAX_LINKS; s++)
            if (s != CARS - 1)
                CHECK(pc_blasts[s].glowIdx == -1 && pc_blasts[s].blastObjs[0].mbIdx == -1
                      && sentinel(&pc_blasts[s].lifeTime, 4, 0, 1));
        /* AnimateNewBlast, the car resurrecting: the glow follows the car, the time runs down. */
        MF(CARS - 1, 0x6C4, int16_t) = -1;
        g_vis[0].slot = CARS - 1;
        entity_update_callback(&g_vis[0], 1);
        CHECK(pc_blasts[CARS - 1].lifeTime < 0.7f && pc_blasts[CARS - 1].mat.pos[1] == PC_CAR_POS(CARS - 1)[1]);
        for (s = 0; s < PC_MAX_LINKS; s++)
            if (s != CARS - 1)
                CHECK(sentinel(&pc_blasts[s].lifeTime, 4, 0, 1) && sentinel(&pc_blasts[s].mat, 4, 0, 12));
    }

    /* Car 11's ring (StartBlast linked to the car): a scene node on the ring's matrix; the ring follows the car. */
    {
        int16_t slot = CARS - 1;
        int first = g_nobj;
        struct { void *next; uint16_t tag; int16_t handle, index; uint8_t pad[10]; intptr_t state; } node;
        save_write_data(&slot, 1, 1.0f, 0);
        CHECK(pc_blast_rings[CARS - 1].ringIdx != -1 && g_obj[first].mat == &pc_blast_rings[CARS - 1].mat);
        CHECK(pc_blast_rings[CARS - 1].ralpha == 240 && pc_blast_rings[CARS - 1].mat.pos[2] == PC_CAR_POS(CARS - 1)[2]);
        PC_CAR_POS(CARS - 1)[0] += 5.0f;
        memset(&node, 0, sizeof node);
        node.index = CARS - 1;
        entity_collision_detect(&node, 1);
        CHECK(pc_blast_rings[CARS - 1].mat.pos[0] == PC_CAR_POS(CARS - 1)[0]);
        CHECK(pc_blast_rings[CARS - 1].incTime < 0.0333334f && pc_blast_rings[CARS - 1].incTime > 0.0f);
        for (s = 0; s < PC_MAX_LINKS; s++)
            if (s != CARS - 1)
                CHECK(pc_blast_rings[s].ringIdx == -1
                      && sentinel(&pc_blast_rings[s].mat, sizeof(struct pc_uv_matrix), 0, 1));
    }

    /* The skids: the table cleared whole, then 12 cars x 4 tires started (each its own segment, at its tire),
     * continued, and stopped. */
    func_800B0580();
    for (k = 0; k < (int)sizeof pc_skids; k++)
        if (((uint8_t *)pc_skids)[k] != 0)
            break;
    CHECK(k == (int)sizeof pc_skids);
    for (s = 0; s < CARS; s++) {
        CF(s, 0xE8, uint32_t) = 0xF;                    /* all four tires' flags */
        MF(s, 0x6C4, int16_t) = -1;                     /* on the ground */
        for (t = 0; t < 4; t++) {
            MF(s, 0x5A0 + 2 * t, uint16_t) = 0;         /* surface 0 */
            MF(s, 0x61C + 2 * t, uint16_t) = 1;         /* terrain 1 */
            MF(s, 0x75C + 4 * t, float) = 0.0f;
        }
    }
    for (s = 0; s < CARS; s++)
        cpak_init(s);
    {
        int ok = 1;
        for (s = 0; s < CARS; s++)
            for (t = 0; t < 4; t++) {
                ok &= pc_skids[s][t].obj != NULL && pc_skids[s][t].pos[0] == PC_CAR_WHEEL_POS(s)[t][0];
                for (int s2 = 0; s2 <= s; s2++)
                    for (int t2 = 0; t2 < 4; t2++)
                        if (s2 != s || t2 != t)
                            ok &= pc_skids[s2][t2].obj != pc_skids[s][t].obj;
            }
        CHECK(ok);
        CHECK(count_list(D_80155220.head) == 4 * CARS);
        for (k = CARS * 4 * (int)sizeof(struct pc_skid); k < (int)sizeof pc_skids; k++)
            if (((uint8_t *)pc_skids)[k] != 0)
                break;
        CHECK(k == (int)sizeof pc_skids);           /* slots 12-15 untouched */
    }
    for (s = 0; s < CARS; s++)
        for (t = 0; t < 4; t++)
            PC_CAR_WHEEL_POS(s)[t][2] += 0.5f;      /* moved along: a continued skid */
    for (s = 0; s < CARS; s++)
        cpak_init(s);
    CHECK(pc_skids[CARS - 1][3].obj != NULL && pc_skids[CARS - 1][3].lenSq > 0.0f);
    for (s = 0; s < CARS; s++)
        CF(s, 0xE8, uint32_t) = 0;
    for (s = 0; s < CARS; s++)
        cpak_init(s);
    for (s = 0; s < CARS; s++)
        for (t = 0; t < 4; t++)
            CHECK(pc_skids[s][t].obj == NULL);
    g_nfreed_dl = 0;
    func_800B0580();
    CHECK(g_nfreed_dl == 4 * CARS && D_80155220.head == NULL);

    /* Car 11's resurrect: its midpoint, raised by the distance / 8 (80 / 8), its own slot only. */
    MF(CARS - 1, 0x6C4, int16_t) = 0;
    MF(CARS - 1, 0x7C6, int16_t) = CARS - 1;
    MF(CARS - 1, 0x714, float) = 0.0f;
    MF(CARS - 1, 0x6C8, float) = 0.0f;
    track_select_handler(&pc_models[CARS - 1]);
    CHECK(pc_resurrect_mid[CARS - 1][0] == 100.0f && pc_resurrect_mid[CARS - 1][1] == 111.0f
          && pc_resurrect_mid[CARS - 1][2] == 102.0f);
    CHECK(UNTOUCHED(pc_resurrect_mid, 0, CARS - 1) && UNTOUCHED(pc_resurrect_mid, CARS, PC_MAX_LINKS));

    if (!test_failures)
        printf("fxcars: the effects' per-car arrays for %d cars of %d slots: resets whole, each car its own slot\n",
               CARS, PC_MAX_LINKS);
    return TEST_DONE();
}
