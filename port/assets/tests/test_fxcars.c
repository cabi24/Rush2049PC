/* port/assets/tests/test_fxcars.c: the effects' per-car arrays (pc_tire_fx, pc_smoke_shade, pc_trail_pts, pc_blasts,
 * pc_blast_rings, pc_skids, pc_resurrect_mid: port/include/pc_game_types.h, docs/port/DECOUPLE.md 6) through the
 * game's own code for a race of 12 cars, slots 0 to 11, with more slots declared than that. This test and its game
 * objects are built with PC_MAX_LINKS 16 (CMakeLists.txt), so each array has 16 entries where the game build has 12
 * and the N64 6. Every entry starts as a sentinel: the resets must reach all 16, and each car's code its own slot only.
 * InitSmoke sets up the puff pool and clears the tire-effect latches; InitBlast (InitBlasts)
 * takes its 50 blast matrices and marks every blast and ring empty; StartSmoke and StartWater start each
 * car's tire puffs (a latch bit, two puffs from the pool) and UpdateSmoke ends them (the latch cleared);
 * StartSmoke's smoke sets and fades a car's grey, its trails keep their last points; InitWingSmoke and
 * InitSmokeBlast clear a car's trail and smoke latches; StartNewBlast, StartBlast (StartBlast,
 * with StartNewBlastRing), AnimateNewBlast and AnimateNewBlastRing (the ring) run car
 * 11's blast and ring, whose matrices the scene nodes point at; InitSkids clears the skid table and UpdateSkids
 * starts, continues and stops the skids of 12 cars x 4 tires; UpdateResurrectingCar sets car 11's resurrect midpoint.
 * game: src/blob/InitSmoke.c src/blob/InitBlast.c
 * game: src/blob/groups/start_smoke/StartSmoke.c src/blob/groups/start_water/StartWater.c
 * game: src/blob/UpdateSmoke.c src/blob/InitWingSmoke.c src/blob/InitSmokeBlast.c src/blob/groups/blast/blast.c
 * game: src/blob/AnimateNewBlast.c src/blob/AnimateNewBlastRing.c src/blob/InitSkids.c
 * game: src/blob/groups/skid_marks/group.c src/blob/StopSkid.c src/blob/UpdateResurrectingCar.c
 * game: src/blob/ClearStatLL.c src/blob/GetStatLLE.c src/blob/FreeStatLLE.c
 * game: src/blob/Random.c src/blob/game_rand.c
 * game: src/blob/ZOID_SetColor.c src/blob/ZOID_RemoveObjectTree.c src/blob/ZOID_RemoveObject.c
 * game: src/blob/groups/mbox_new_object/group.c src/blob/AddToEnvList.c
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
#define MF(i, off, T) (*(T *)((uint8_t *)&pc_models[i] + MODEL_OFS(off)))   /* model i's field at MAT3's offset */

/* A StatLL list header, as every view has it on the PC (40 bytes): the element array, the active and free lists. */
typedef struct { int8_t dbl; int32_t count, size; void *list, *head, *store; } StatLL;
typedef struct Puff {   /* UpdateSmoke's Puff: 88 bytes on the N64, 96 here */
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
StatLL gSmokeList __attribute__((aligned(16)));   /* the puff pool, its active list head */
Puff gSmokeObjs[100];
StatLL gSkidList;                                /* the skid segment pool */
EffectObj gSkidArray[100];
uint8_t ObjInstanceList[64 * 104] __attribute__((aligned(16)));   /* the scene nodes, 104 bytes each on the PC */

int8_t expansion_pak_present = 1, gTrackNum, gGoodFrameRate = 1, gSuperSpeed, gGameSoundsEnabled;
int16_t gNumPlayers = 1, gBlastMatCnt;
int32_t gPauseState, rand_seed = 1, start_water_sentinel, gGameState;
uint8_t gNumModels = 1;
volatile float gFrameSeconds = 1.0f / 60;
float gModelTime, resurrectTime = 1.0f, gCarMiddles[13], unitmatrix[4][3], skid_lift = 0.8f, skid_min_length_sq = 0.1f;
float gSeconds[4], gDummyVtx[16];
uint16_t gFireTexs[6], gObjList[220], gTexList[10];   /* the smoke, fire and spray models from 195; the skid texture, 8 */
char *FireTexNames[6];
int8_t gCarWings[(PC_MAX_LINKS + 1) * 13];   /* row 0 the default, then each player's option per car type ([13][13] in the game) */
union { uint8_t c[4]; uint32_t rgba; } smoke_white, smoke_wing_color1, smoke_wing_color2, smoke_wing_color3, smoke_road_color, smoke_dirt_color;   /* the game's tRGBA */
struct { struct tTexture *data; int32_t count; } gTexDefPtrs[64];   /* TextureDefTable */
void *gBlastMats[50];
void *gEnvList;
uint32_t skid_tire_flags[4] = { 1, 2, 4, 8 };
uint8_t gDirtColor[4] = { 1, 2, 3, 4 }, gShadowColor[4] = { 5, 6, 7, 8 };
struct { uint8_t pad[2]; uint16_t flags; uint8_t pad4[0x14]; } g_surface[4];
void *stree_surf = g_surface;
struct { float uvs[3][3], pos[3]; } gCamera;

/* ---------------------------------------------------------------- what the code under test calls */

static float g_mats[50][12];
static int g_nmats;
void *ZOID_NewMatrix(void) { return g_mats[g_nmats++ % 50]; }   /* GetMatrix */

static struct { int32_t obj; const void *mat; } g_obj[256];
static int g_nobj;
int32_t ZOID_AddObject(int32_t obj, void *mat, int16_t parent, int32_t flags)   /* ZOID_AddObject: a scene node */
{
    (void)parent; (void)flags;
    if (g_nobj < 256) g_obj[g_nobj].obj = obj, g_obj[g_nobj].mat = mat;
    return 1 + g_nobj++ % 60;
}
static struct {   /* a visual (tVisual, struct pc_visual), 0x28 bytes on the PC */
    void *next;
    int16_t index, objnum, slot;
    uintptr_t data;
    float timeStamp;
    void *func;
} g_vis[16];
static int g_nvis;
void *GrabEnvEntry(void) { return g_nvis < 16 ? &g_vis[g_nvis++] : NULL; }   /* GrabEnvEntry */
static int g_nremoved;
void RemoveObjectTree(int16_t obj, int32_t a, int32_t b) { (void)obj; (void)a; (void)b; g_nremoved++; }
void ReleaseEnviron(void *v, int32_t unlink) { (void)v; (void)unlink; }
void CopyUV(void *src, void *dst) { (void)src; (void)dst; }
void LookInDir(void *v, void *m) { (void)v; (void)m; }
void vector_direction(float *in, float *out) { out[0] = in[0], out[1] = in[1], out[2] = in[2]; }
float NormalVector(float *v) { (void)v; return 1.0f; }
float vector_magnitude(float *v) { (void)v; return 80.0f; }
void MBOX_HideObject(int32_t h, int32_t m, uint32_t v) { (void)h; (void)m; (void)v; }
void MBOX_ShowObject(int32_t h, int32_t m, uint32_t v) { (void)h; (void)m; (void)v; }
void ZOID_SetTexture(int16_t h, void *r, int32_t m) { (void)h; (void)r; (void)m; }
void MBOX_FindTexture_WORK(char *a, int16_t *b, int c, int d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
void newWorldDamage(float *pos, int16_t slot) { (void)pos; (void)slot; }
void ZOID_SetObjectDef(int16_t obj, uint16_t def) { (void)obj; (void)def; }
int EmitGameSound(const float *p, intptr_t d, float a, float b, float c, float e, int f, int g, int h,
                           unsigned char i) { (void)p; (void)d; (void)a; (void)b; (void)c; (void)e; (void)f; (void)g;
                           (void)h; (void)i; return 0; }
void PitchUV(float a, float uv[3][3]) { (void)a; (void)uv; }
void YawUV(float a, float uv[3][3]) { (void)a; (void)uv; }
static int g_ndl, g_nfreed_dl;
static int g_dls[128];
void *ZOID_NewPoly(int32_t n, float *a, uint16_t b, uint8_t *c, int32_t d, int32_t e)
{
    (void)n; (void)a; (void)b; (void)c; (void)d; (void)e;
    return &g_dls[g_ndl++ % 128];
}
void ZOID_RemovePoly(void *dl) { (void)dl; g_nfreed_dl++; }
void ZOID_UpdatePoly(void *dl, int32_t n, float *quad, int32_t a, uint8_t *c, int32_t b, int32_t d)
{ (void)dl; (void)n; (void)quad; (void)a; (void)c; (void)b; (void)d; }
void Interpolate(float *a, float *b, int16_t n, float lo, float hi, float t, float *out)
{
    (void)a; (void)b; (void)lo; (void)hi; (void)t;
    for (int k = 0; k < n; k++) out[k] = 100.0f + k;
}
void MakeUvsFromQuat(float *quat, float uvs[3][3]) { (void)quat; (void)uvs; }
void make_super_speed(void *m) { (void)m; }
void *n64_memset(void *p, int c, uint32_t n) { return memset(p, c, n); }

void InitSmoke(void);
void InitBlast(void);
void StartSmoke(int16_t slot, uint32_t type, int32_t fast, float *offset);
void UpdateSmoke(void);
void InitWingSmoke(int16_t slot, int16_t which);
void InitSmokeBlast(int16_t slot);
void StartNewBlast(int16_t slot);
void StartBlast(void *data, int linkToCar, float scale, int playSound);
void AnimateNewBlast(void *v, int16_t op);
void AnimateNewBlastRing(void *node, int16_t update);
void InitSkids(void);
void UpdateSkids(int32_t slot);
void UpdateResurrectingCar(void *m);

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

    static struct tTexture texdefs[64];   /* a model's textures (the WAD's records) */
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
    InitSmoke();
    CHECK(gSmokeList.list == gSmokeObjs && gSmokeList.count == 100 && gSmokeList.size == (int32_t)sizeof(Puff));
    for (s = 0; s < PC_MAX_LINKS; s++)
        CHECK(pc_tire_fx[s] == 0);

    /* InitBlasts: 50 matrices from GetMatrix, whatever the cars, and every blast and ring marked empty, the rest of
     * their bytes left alone. */
    g_nmats = 0;
    InitBlast();
    CHECK(g_nmats == 50 && gBlastMats[49] == g_mats[49] && gBlastMatCnt == 0);
    for (s = 0; s < PC_MAX_LINKS; s++) {
        CHECK(pc_blast_rings[s].ringIdx == -1 && sentinel(&pc_blast_rings[s].mat, sizeof pc_blast_rings[s].mat, 0, 1));
        CHECK(pc_blasts[s].glowIdx == -1 && sentinel(&pc_blasts[s].mat, sizeof pc_blasts[s].mat, 0, 1));
        for (k = 0; k < 4; k++)
            CHECK(pc_blasts[s].blastObjs[k].mbIdx == -1 && pc_blasts[s].blastObjs[k].ht != 0.0f);
    }

    /* Each car's front-left tire puffs: its latch bit 0x100 and two puffs from the pool, for its own slot. */
    for (s = 0; s < CARS; s++)
        StartSmoke(s, 0, 1, NULL);
    for (s = 0; s < CARS; s++)
        CHECK(pc_tire_fx[s] == 0x100);
    for (s = CARS; s < PC_MAX_LINKS; s++)
        CHECK(pc_tire_fx[s] == 0);
    CHECK(count_list(gSmokeList.head) == 2 * CARS);
    StartSmoke(CARS - 1, 0, 1, NULL);            /* latched: no more puffs */
    CHECK(count_list(gSmokeList.head) == 2 * CARS);
    {
        int cars_seen[CARS] = { 0 };
        for (Puff *p = gSmokeList.head; p; p = p->next)
            if (p->car >= 0 && p->car < CARS && p->flags == 0x10)
                cars_seen[p->car]++;
        for (s = 0; s < CARS; s++)
            CHECK(cars_seen[s] == 2);
    }
    /* UpdateSmoke ends them (the tires' visual code isn't a puff's): every car's latch bit cleared, the puffs freed. */
    UpdateSmoke();
    for (s = 0; s < PC_MAX_LINKS; s++)
        CHECK(pc_tire_fx[s] == 0);
    CHECK(gSmokeList.head == NULL);

    /* Car 11's smoke: the latches 0x40, 0x20 and 0x10, its grey set to 175 and faded by 3 a puff; then two frames
     * latched (0x40 to 0x80, 0x80 off), then another puff, faded again. */
    StartSmoke(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x40 | 0x20 | 0x10) && pc_smoke_shade[CARS - 1] == 172);
    StartSmoke(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x80 | 0x20 | 0x10));
    StartSmoke(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x20 | 0x10));
    StartSmoke(CARS - 1, 4, 0, NULL);
    CHECK(pc_tire_fx[CARS - 1] == (0x40 | 0x20) && pc_smoke_shade[CARS - 1] == 169);
    CHECK(UNTOUCHED(pc_smoke_shade, 0, CARS - 1) && UNTOUCHED(pc_smoke_shade, CARS, PC_MAX_LINKS));
    for (s = 0; s < CARS - 1; s++)
        CHECK(pc_tire_fx[s] == 0);
    InitSmokeBlast(CARS - 1);                            /* the smoke's 0x20 */
    CHECK(pc_tire_fx[CARS - 1] == 0x40);

    /* Car 11's two trails: their first points kept (0x4000, 0x8000), each in its half of the car's six floats.
     * (The game starts the trails for the one human only, AnimateWing; the slot here shows the storage's.) */
    {
        float offset[3] = { 0.0f, 0.0f, 0.0f };
        gNumPlayers = 1;
        StartSmoke(CARS - 1, 6, 0, offset);
        CHECK((pc_tire_fx[CARS - 1] & 0x4000) && !(pc_tire_fx[CARS - 1] & 0x8000));
        CHECK(pc_trail_pts[CARS - 1][0] != 0.0f && UNTOUCHED(pc_trail_pts[CARS - 1], 3, 6));
        StartSmoke(CARS - 1, 7, 0, offset);
        CHECK(pc_tire_fx[CARS - 1] & 0x8000);
        CHECK(!sentinel(pc_trail_pts[CARS - 1], sizeof(float), 3, 6));
        CHECK(UNTOUCHED(pc_trail_pts, 0, CARS - 1) && UNTOUCHED(pc_trail_pts, CARS, PC_MAX_LINKS));
        InitWingSmoke(CARS - 1, 0);                     /* the left trail's 0x4000 */
        InitWingSmoke(CARS - 1, 1);                     /* the right's 0x8000 */
        CHECK(!(pc_tire_fx[CARS - 1] & 0xC000));
    }

    /* Car 11's blast: four blast objects and the glow, each a scene node on its matrix in the car's record. */
    {
        int first = g_nobj;
        StartNewBlast(CARS - 1);
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
        AnimateNewBlast(&g_vis[0], 1);
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
        StartBlast(&slot, 1, 1.0f, 0);
        CHECK(pc_blast_rings[CARS - 1].ringIdx != -1 && g_obj[first].mat == &pc_blast_rings[CARS - 1].mat);
        CHECK(pc_blast_rings[CARS - 1].ralpha == 240 && pc_blast_rings[CARS - 1].mat.pos[2] == PC_CAR_POS(CARS - 1)[2]);
        PC_CAR_POS(CARS - 1)[0] += 5.0f;
        memset(&node, 0, sizeof node);
        node.index = CARS - 1;
        AnimateNewBlastRing(&node, 1);
        CHECK(pc_blast_rings[CARS - 1].mat.pos[0] == PC_CAR_POS(CARS - 1)[0]);
        CHECK(pc_blast_rings[CARS - 1].incTime < 0.0333334f && pc_blast_rings[CARS - 1].incTime > 0.0f);
        for (s = 0; s < PC_MAX_LINKS; s++)
            if (s != CARS - 1)
                CHECK(pc_blast_rings[s].ringIdx == -1
                      && sentinel(&pc_blast_rings[s].mat, sizeof(struct pc_uv_matrix), 0, 1));
    }

    /* The skids: the table cleared whole, then 12 cars x 4 tires started (each its own segment, at its tire),
     * continued, and stopped. */
    InitSkids();
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
        UpdateSkids(s);
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
        CHECK(count_list(gSkidList.head) == 4 * CARS);
        for (k = CARS * 4 * (int)sizeof(struct pc_skid); k < (int)sizeof pc_skids; k++)
            if (((uint8_t *)pc_skids)[k] != 0)
                break;
        CHECK(k == (int)sizeof pc_skids);           /* slots 12-15 untouched */
    }
    for (s = 0; s < CARS; s++)
        for (t = 0; t < 4; t++)
            PC_CAR_WHEEL_POS(s)[t][2] += 0.5f;      /* moved along: a continued skid */
    for (s = 0; s < CARS; s++)
        UpdateSkids(s);
    CHECK(pc_skids[CARS - 1][3].obj != NULL && pc_skids[CARS - 1][3].lenSq > 0.0f);
    for (s = 0; s < CARS; s++)
        CF(s, 0xE8, uint32_t) = 0;
    for (s = 0; s < CARS; s++)
        UpdateSkids(s);
    for (s = 0; s < CARS; s++)
        for (t = 0; t < 4; t++)
            CHECK(pc_skids[s][t].obj == NULL);
    g_nfreed_dl = 0;
    InitSkids();
    CHECK(g_nfreed_dl == 4 * CARS && gSkidList.head == NULL);

    /* Car 11's resurrect: its midpoint, raised by the distance / 8 (80 / 8), its own slot only. */
    MF(CARS - 1, 0x6C4, int16_t) = 0;
    MF(CARS - 1, 0x7C6, int16_t) = CARS - 1;
    MF(CARS - 1, 0x714, float) = 0.0f;
    MF(CARS - 1, 0x6C8, float) = 0.0f;
    UpdateResurrectingCar(&pc_models[CARS - 1]);
    CHECK(pc_resurrect_mid[CARS - 1][0] == 100.0f && pc_resurrect_mid[CARS - 1][1] == 111.0f
          && pc_resurrect_mid[CARS - 1][2] == 102.0f);
    CHECK(UNTOUCHED(pc_resurrect_mid, 0, CARS - 1) && UNTOUCHED(pc_resurrect_mid, CARS, PC_MAX_LINKS));

    if (!test_failures)
        printf("fxcars: the effects' per-car arrays for %d cars of %d slots: resets whole, each car its own slot\n",
               CARS, PC_MAX_LINKS);
    return TEST_DONE();
}
