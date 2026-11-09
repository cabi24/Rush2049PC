/* port/assets/tests/test_targets.c: the target info table (targetInfo, 0x40 bytes a record on the PC) and the
 * level-object nodes, the targets (struct tTarget, 0x78 bytes in the host layout, the pool pc_target_nodes: PC data,
 * linked by host pointers, their kinds' records pointers; docs/port/POINTERS.md 10.2), through the game's own code:
 * the node pool's set-up (initTargetStructs, ClearStatLL), the instantiation and per-node set-up (targets/group.c:
 * SetupTarget, SetTargetDefs with the target groups' side table, SetTargetDef), the start functions the table names
 * (StartWindow, StartKnockdown, StartCone, StartFence, StartCactus) and a hit function (StartCoin), the target visuals
 * (AnimateKnockdown, AnimateRotGeneric, AnimateObjSwapGeneric, the debris AnimateVelRotGeneric, which frees its target
 * into the pool), the collision pass that calls a target's hit function (CheckTargetCollision), the path targets
 * (StartPathTarget, GetTargetVelocity, TargetCollTrigger: the path record and the PTHD record, which keep their N64
 * layout with window addresses) and a bounding box model's collision (TargetCollBBox and its response
 * setMiniFBCollisionForce: the model keeps its box as a row of BBoxCollInfo). Records past the first are used
 * throughout, so an N64 stride or offset lands somewhere else. The heap records and the track records sit in an
 * RDRAM window the test maps (exit 77, skipped, if it can't).
 * game: src/blob/initTargetStructs.c src/blob/ClearStatLL.c src/blob/GetStatLLE.c
 * game: src/blob/FreeStatLLE.c src/blob/groups/targets/group.c src/blob/UpdateMiniModelStruct.c
 * game: src/blob/StartWindow.c src/blob/StartKnockdown.c src/blob/StartCone.c src/blob/StartFence.c
 * game: src/blob/StartCactus.c src/blob/StartCoin.c src/blob/AnimateKnockdown.c src/blob/AnimateRotGeneric.c
 * game: src/blob/AnimateObjSwapGeneric.c src/blob/AnimateVelRotGeneric.c src/blob/CheckTargetCollision.c
 * game: src/blob/groups/targetpaths/group.c src/blob/GetTargetVelocity.c src/blob/TargetCollTrigger.c
 * game: src/blob/TargetCollBBox.c src/blob/setMiniFBCollisionForce.c
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

/* A StatLL header on the PC (tStatList, host pointers). */
typedef struct { uint8_t doubly; int32_t count, size; void *mem, *active, *free; } StatLL;

#define NF(n, off, T) (*(T *)((uint8_t *)(n) + (off)))   /* a record's field at its offset (the N64-layout records) */
#define W32(p) ((uint32_t)(uintptr_t)(p))
#define WP(a) ((void *)(uintptr_t)(a))
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data */

struct tTarget pc_target_nodes[PC_TARGET_NODES];   /* the nodes (pc_game_types.h), which data.c defines */
#define NODE(i) (&pc_target_nodes[i])
TEST_ABS(BBoxCollInfo);              /* the bounding boxes, a row of eight floats each (arena data) */
struct pc_model pc_models[PC_MODEL_SLOTS];    /* model[] (pc_game_types.h), which data.c defines */
#define MODEL(i) ((uint8_t *)&pc_models[i])
#define HEAP TEST_SCRATCH             /* the game heap (AllocateMemoryHeap), the track's records */

TInfo targetInfo[122];
StatLL gTargetList, gAlwaysColList;
typedef void (*HitFunc)(void *, void *, void *, int32_t);
int32_t (*gTargetCollFuncPtrs[6])(void *, void *, void *, int32_t);
uint8_t gTargetStructSizes[8], gTargetStructCount[8];
uint8_t *gTargetStructPtrs[8], *gTargetOwnStructPtrs[8], *gTargetStructInfoPtrs[8];
int32_t gBBoxModelCount, gBBoxModelUsed, gSilverCoinCnt, gGoldCoinCnt, gWhichSilverCoin, gWhichGoldCoin, gBattleMineCnt, gPathCount;
void *gBBoxModelStructs;
intptr_t gPlayerMines[4][5];
void *gAlwaysCol;
struct { void *paths; int32_t count; } gTargetPaths;   /* paths, Scene36 *: the PTHD scenes, a track WAD record */
struct pc_target_group *gTargetGroupList;   /* GTLD's side table (pc_game_types.h) */
int32_t numgTargetGroupList;
uint32_t gGameState, gGameState;
int8_t expansion_pak_present, Restarting, gRestartGame, gBackwards, gMineCones, gCarMines;
int32_t gRace, gRace, gPauseState, numCollisionTargetPtrs;
int16_t gNumPlayers;
volatile int16_t gNumCars;         /* the cars in the race (TargetCollTrigger's cars from 8 on) */
volatile uint8_t gNumModels = 2;
uint16_t gObjList[1024];   /* textures: the camera record names 361 */
float unitmatrix[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
float zero_velocity[3], velrot_gravity[3] = {0.0f, -0.25f, 0.0f}, unitmatrix_origin[3];
float cactus_frame_time = 0.75f, fence_frame_time = 1.25f;
volatile float gFrameSeconds = 0.5f;
int8_t gGameSoundsEnabled;
struct tTarget *CollisionTargetPtrs[30];
void *gTargetGroups;                    /* the collision quadtree (a track record) */
void *stree_surf;                    /* the road polygons */
void *gEnvList;
uint8_t ObjInstanceList[600 * 0x68];      /* the scene nodes (host layout: the model id at +0x18) */
struct pc_car pc_cars[PC_CAR_SLOTS];   /* the car records (pc_game_types.h), which data.c defines */
#define CAR(i) ((uint8_t *)&pc_cars[i])

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
int32_t ZOID_AddObject(int32_t key, void *mat, int16_t parent, int32_t flags) { return g_handle++; }
int32_t MBOX_FindObject_WORK(const char *name, int8_t a, int8_t b, int8_t c) { return (int32_t)strlen(name) * 16; }
int game_strncmp(const char *a, const char *b, unsigned int n) { return strncmp(a, b, n); }
int n64_strlen(const char *s) { return (int)strlen(s); }
char *game_strstr(char *s, char *find) { return strstr(s, find); }
void CopyUV(float *src, float *dst) { memcpy(dst, src, 9 * sizeof(float)); }
int32_t FindWorldGroup(float *pos) { return -1; }
float MBOX_GetObjectRadius(int32_t key) { return 100.0f; }
void MBOX_GetPosition(void *xform, int32_t handle) {}
uint32_t ZOID_ObjectFlags(int16_t id) { return 0; }
void InitBattlePowerUps(int32_t a) {}
void ResetCollisionObject(int32_t a) {}
void RemoveCollisionObject(uint32_t a) {}
void InitCollisionTargetPtrs(void) {}
void PreInitBattlePowerUps(void) {}
static struct pc_visual g_visuals[16];
static int g_nvisual;
void *GrabEnvEntry(void)
{
    struct pc_visual *v = &g_visuals[g_nvisual++ & 15];
    memset(v, 0, sizeof *v);
    return v;
}
static void *g_released[8];
static int g_nreleased;
void ReleaseEnviron(void *v, int32_t unlink) { g_released[g_nreleased++ & 7] = v; }
static int16_t g_spawned[8];
static int g_nspawned;
void RemoveObjectTree(int16_t id, int32_t a, int32_t b) { g_spawned[g_nspawned++ & 7] = id; }
static int32_t g_sound;
static void *g_sound_pos;
int32_t PlayTargetSound(int32_t sound, int32_t slot, void *pos, uint8_t mode) { g_sound = sound, g_sound_pos = pos; return 0; }
static void *g_rate, *g_uvs;
static float g_rate_copy[3];   /* what g_rate pointed to then: AnimateRotGeneric passes its own frame's floats */
void RotateUV(void *rate, void *uvs)
{
    g_rate = rate, g_uvs = uvs;
    memcpy(g_rate_copy, rate, sizeof g_rate_copy);
}
void YawUV(float a, void *pos) {}
void LookInDir(void *v, void *m) {}
void StartBlast(void *pos, int32_t a, float s, int32_t b) {}
void MBOX_HideObject(int32_t obj, int32_t a, int32_t b) {}
int32_t IsCoinFound(int32_t car, int32_t n) { return 0; }
static int g_interp;
void FoundCoin(int32_t car, int32_t n) { g_interp = n; }
void BattleDamage(void *a, void *b, int32_t c) {}
void BodyVector(float *v, float *out, float *m)
{
    for (int i = 0; i < 3; i++)
        out[i] = m[i * 3] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
}
void WorldVector(float *in, float *out, float *m)
{
    for (int j = 0; j < 3; j++)
        out[j] = in[0] * m[j] + in[1] * m[3 + j] + in[2] * m[6 + j];
}
static void *g_hit_other;
void MiniForceApart(void *car, float *dir) { g_hit_other = car; }   /* setMiniFBCollisionForce's push out of the record's box */
float vector_magnitude(float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
int8_t gSuicideMode;
/* the scripted cameras */
int32_t AudEmitEffect(float *pos, intptr_t up, float f2C, float a, float b, float c, int32_t id, int32_t d,
                            int32_t s28, int32_t e) { return 40 + id; }
void AudUpdateEmitter(int32_t h, intptr_t pos, intptr_t up, float a, float b) {}
int32_t AudEmitterPlaying(int32_t h) { return 1; }
void AudStopEmitter(int32_t h) {}
void CreateQuatMat(float *m, float *rot, int32_t a) {}
void InterpQuats(float t, float *a, float *b, float *out) {}
void MoveCollisionObject(int32_t id, float *v) {}
void TransposeUV(float *m) {}
void RotateCollisionObject(int32_t id, float *pos, float *mat, float *m) {}

/* the table's functions */
static void *g_started;
static void start_fn(void *t) { g_started = t; }
static void visual_fn(void *v, short op) {}
static void camera_visual_fn(void *v, short op) {}
static int32_t g_hit_calls;
static int32_t hit_fn(void *a, void *b, void *c, int32_t d) { g_hit_calls++; return 1; }

void initTargetStructs(int32_t a);
int32_t SetupTarget(void *rec, int16_t parent, float *pos, void *src, int8_t index, int32_t place,
                               int32_t other);
void SetTargetDefs(void);
void StartWindow(void *t);
void StartKnockdown(void *t);
void StartCone(void *t);
void StartFence(void *t);
void StartCactus(void *t);
void StartCoin(void *t);
void AnimateKnockdown(void *v, int16_t op);
void AnimateRotGeneric(void *v, int16_t op);
void AnimateObjSwapGeneric(void *v, int16_t op);
void AnimateVelRotGeneric(void *v, int16_t op);
void CheckTargetCollision(int16_t player);
void StartPathTarget(void *cam);
void GetTargetVelocity(int32_t id, float *out);
int32_t TargetCollTrigger(void *cam, int16_t *player, int32_t a, int32_t b);
int32_t TargetCollBBox(void *object, int16_t *index, void *a, int32_t b);
void setMiniFBCollisionForce(int32_t a, void *car, void *other, float *dir, float *pos);

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

    memset(targetInfo, 0, sizeof targetInfo);
    for (int i = 0; i < 122; i++)
        targetInfo[i].name = "UNUSED", targetInfo[i].file = "none";
    t = &targetInfo[CONE];
    t->name = "CONE", t->file = "cone", t->start = start_fn, t->visual = visual_fn;
    t->kind = 1, t->flags = 0x2 | 0x4 | 0x8 | 0x20, t->shape = 3, t->category = 2, t->value = 3.5f;
    t->sound = 0x77;
    t = &targetInfo[CAMERA];
    t->name = "CAMERA", t->file = "cam", t->start = start_fn, t->visual = camera_visual_fn;
    t->kind = 4, t->flags = 0x8 | 0x20 | 0x8000, t->shape = 361, t->category = 4, t->value = 2.0f;
    t->sound = -1, t->track = -1, t->trackB = -1;
    t = &targetInfo[DEBRIS];
    t->name = "DEBRIS", t->flags = 0x2000;
    gTargetStructSizes[2] = 24, gTargetStructSizes[4] = 72;   /* the category records: cone 24 bytes, camera CamCtl 72 */
}

static void setup_track(void)
{
    uint8_t *sc = WP(SCENES), *k = WP(KEYS), *place = WP(PLACE);

    memset(sc, 0, 0x400);
    gTargetPaths.paths = WP(SCENES);
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
    gTargetStructInfoPtrs[2] = WP(STATIC);
}

/* ---------------------------------------------------------------- tests */

/* initTargetStructs: 130 nodes of the host type, linked by host pointers, their fields after the link untouched. */
static void test_pool(void)
{
    CHECK(sizeof(struct tTarget) == 0x78 && offsetof(struct tTarget, data) == 0x70 && sizeof pc_target_nodes == 130 * 0x78);
    CHECK(offsetof(struct tTarget, mat.pos) == 0x3C && offsetof(struct tTarget, pos) == 0x48 && offsetof(struct tTarget, slot) == 0x60);
    memset(pc_target_nodes, 0xEE, sizeof pc_target_nodes);
    memset(&gTargetList, 0, sizeof gTargetList);
    gRace = 0;
    initTargetStructs(0);
    CHECK(gTargetList.count == 130 && gTargetList.size == (int32_t)sizeof(struct tTarget) && gTargetList.doubly == 0);
    CHECK(gTargetList.mem == NODE(0) && gTargetList.free == NODE(0) && gTargetList.active == NULL);
    CHECK(NODE(0)->next == NODE(1) && NODE(1)->next == NODE(2));
    CHECK(NODE(128)->next == NODE(129) && NODE(129)->next == NULL);
    CHECK(NODE(0)->Flags == 0xEE && NODE(1)->data == (void *)(uintptr_t)0xEEEEEEEEEEEEEEEEull);
    memset(pc_target_nodes, 0, sizeof pc_target_nodes);
    initTargetStructs(0);
}

/* Instantiate a cone (with its place record) and a camera (scene 1), then set them up. */
static void test_instantiate(void)
{
    struct tTarget *cone, *cam;
    uint8_t *ctl;
    struct pc_visual *v;
    float zero[3] = {0, 0, 0};
    StatLL *pool = &gTargetList;

    expansion_pak_present = 1, gNumPlayers = 1, gGameState = 0, gTargetPaths.count = 0, numgTargetGroupList = 0, g_nvisual = 0, g_started = NULL;
    gObjList[3] = 0x1234;
    CHECK(SetupTarget(WP(PLACE), -1, zero, WP(PLACE), 0, 1, 1) == 1);
    strcpy((char *)WP(PLACE + 0x100), "NOTHING");
    CHECK(SetupTarget(WP(PLACE + 0x100), -1, zero, NULL, 1, 1, 1) == 0);   /* no such kind */
    strcpy((char *)WP(PLACE + 0x100), "CAMERA_1");
    CHECK(SetupTarget(WP(PLACE + 0x100), -1, zero, NULL, 1, 1, 1) == 1);
    cone = NODE(0), cam = NODE(1);
    CHECK(pool->active == cam && cam->next == cone && cone->next == NULL);
    CHECK(pool->free == NODE(2) && NODE(2)->next == NODE(3));
    CHECK(cone->infoIdx == CONE && cam->infoIdx == CAMERA && cone->owner == 0);
    CHECK(cone->radius == 3.5f && cone->otype == 3 && cone->dlIdx == 7);
    CHECK(cone->mbIdx == 0x4242 && cam->mbIdx == 0x5151 && (cam->Flags & 8));
    CHECK(cone->mat.pos[0] == 30.0f && cone->mat.pos[2] == -20.0f);
    CHECK(gTargetStructCount[2] == 1 && gTargetStructCount[4] == 1 && gBBoxModelCount == 1);

    SetTargetDefs();
    ctl = cam->data;                                                 /* the camera's path record, a heap block */
    CHECK(W32(gTargetStructPtrs[2]) >= HEAP && cone->data == gTargetStructPtrs[2]);
    CHECK(gTargetOwnStructPtrs[2] == gTargetStructPtrs[2] + 24 && ctl == gTargetStructPtrs[4]);
    CHECK(ctl && NF(ctl, 0, uint32_t) == SCENES + 36);              /* its path: &gTargetPaths.paths[1] (N64 form) */
    CHECK(W32(gBBoxModelStructs) >= HEAP && cam->owner == 0 && gBBoxModelUsed == 1);
    CHECK(NF(gBBoxModelStructs, 0, int32_t) == 2);                  /* its box: row 2 of BBoxCollInfo */
    CHECK(NF(gBBoxModelStructs, 4, float) == 1.0f && NF(gBBoxModelStructs, 0x14, float) == 1.0f);
    CHECK(NF(ObjInstanceList + 7 * 0x68, 0x18, uint16_t) == 0x1234);    /* the cone's texture (N64 +0x14) */
    v = (struct pc_visual *)gEnvList;                              /* the cone's animation */
    CHECK(v == &g_visuals[0] && v->data == (uintptr_t)cone && v->func == visual_fn && v->timeStamp == 0.0f);
    CHECK(g_started == cone);                                        /* flags & 4: its start function */
}

/* The start functions: a visual with the table's visual function and the target, and the table's sound. */
static void test_start(void)
{
    struct tTarget *t = NODE(0);
    uint8_t *car = CAR(1);
    struct pc_visual *v;
    uint8_t *motion = WP(HEAP + 0x900u);

    memset(car, 0, PC_CAR_SIZE);
    NF(car, 0x14, float) = 8.0f, NF(car, 0x18, float) = 16.0f, NF(car, 0x1C, float) = 24.0f;
    t->slot = 1;
    t->mat.uvs[2][0] = 1.0f;                     /* the way it faces (StartWindow) */

    gEnvList = NULL, g_nvisual = 0, g_sound = 0;
    StartWindow(t);
    v = &g_visuals[0];
    CHECK(gEnvList == v && v->func == visual_fn && v->data == (uintptr_t)t && v->timeStamp == 0.0333333f);
    CHECK(t->frames == 7 && g_sound == 0x77 && g_sound_pos == t->mat.pos && !(t->Flags & 6));

    g_sound = 0;
    StartKnockdown(t);
    v = &g_visuals[1];
    CHECK(gEnvList == v && v->next == &g_visuals[0] && v->func == visual_fn && v->data == (uintptr_t)t);
    CHECK(t->frames == 4 && g_sound == 0x77);

    g_sound = 0;
    StartFence(t);
    v = &g_visuals[2];
    CHECK(gEnvList == v && v->func == visual_fn && v->data == (uintptr_t)t && v->timeStamp == 1.25f && v->index == 0);
    CHECK(t->frames == 0xC && g_sound == 0x77);

    g_sound = 0;
    StartCactus(t);
    v = &g_visuals[3];
    CHECK(gEnvList == v && v->func == visual_fn && v->data == (uintptr_t)t && v->timeStamp == 0.75f);
    CHECK(t->frames == 7 && g_sound == 0x77);

    memset(motion, 0, 24);
    t->data = motion;
    targetInfo[CONE].shape = 3;
    g_sound = 0;
    StartCone(t);
    v = &g_visuals[4];
    CHECK(gEnvList == v && v->func == visual_fn && v->data == (uintptr_t)t && v->timeStamp == 5.0f);
    CHECK(NF(motion, 0xC, float) == 1.0f && NF(motion, 0x10, float) == 4.0f && NF(motion, 0x14, float) == 3.0f);
    CHECK(NF(motion, 4, float) == 12.0f && NF(motion, 8, float) == 15.0f && g_sound == 0x77);
}

/* A hit function: its state is the target's data, the coin's record (a heap block in the N64 layout). */
static void test_hit(void)
{
    struct tTarget *t = NODE(0);
    uint8_t *state = WP(HEAP + 0xA00u);

    memset(state, 0, 0x50);
    NF(state, 0x4A, uint16_t) = 9, NF(state, 0x48, uint16_t) = 1;   /* car 0's slot bit */
    t->data = state;
    t->slot = 0;
    NF(CAR(0), CAR_OFS(0x35C), int8_t) = 0;
    g_interp = 0, g_sound = 0;
    StartCoin(t);
    CHECK(g_interp == 9 && NF(state, 0x48, uint16_t) == 0 && (t->Flags & 0x80) && g_sound == 0x77);
}

/* The target visuals: each finds its target through the visual's data word, and the target's record through its
 * data. */
static void test_visuals(void)
{
    struct tTarget *t = NODE(2);
    uint8_t *rate = WP(HEAP + 0xB00u);
    struct pc_visual v;

    memset(t, 0, sizeof *t);
    memset(&v, 0, sizeof v);
    v.data = (uintptr_t)t;
    t->data = rate;
    NF(rate, 0, float) = 2.0f, NF(rate, 4, float) = 4.0f, NF(rate, 8, float) = 6.0f;

    t->frames = 2, g_nreleased = 0;
    AnimateKnockdown(&v, 1);
    CHECK(g_rate == rate && g_uvs == t->mat.uvs && t->frames == 1 && g_nreleased == 0);
    AnimateKnockdown(&v, 1);
    CHECK(t->frames == 0 && g_nreleased == 1 && g_released[0] == &v);

    g_uvs = NULL;
    AnimateRotGeneric(&v, 1);
    CHECK(g_uvs == t->mat.uvs && g_rate != rate);   /* rate * frame time, on the stack */
    CHECK(g_rate_copy[0] == 1.0f && g_rate_copy[2] == 3.0f);   /* g_rate itself points into a returned frame */

    /* the frame stepper: the last frame of a kind with flag 0x2000 removes the object */
    t->infoIdx = DEBRIS, t->dlIdx = 33;          /* the handle; its low half is the model id */
    t->otype = 4, t->frames = 2, t->type = 4;
    v.index = 1, v.timeStamp = 0.0f, g_nspawned = 0, g_nreleased = 0;
    AnimateObjSwapGeneric(&v, 1);
    CHECK(g_nspawned == 1 && g_spawned[0] == 33 && g_nreleased == 1 && g_released[0] == &v);
}

/* The debris: its physics state moves it, and when its time is up it goes back to the pool. */
static void test_debris(void)
{
    StatLL *pool = &gTargetList;
    struct tTarget *t = NODE(1);
    uint8_t *phys = WP(HEAP + 0xC00u);
    struct pc_visual v;
    void *free_before = pool->free;

    memset(phys, 0, 24);
    t->data = phys;
    t->infoIdx = DEBRIS, t->dlIdx = 44, t->mat.pos[0] = 1.0f;
    memset(&v, 0, sizeof v);
    v.data = (uintptr_t)t, v.timeStamp = 1.0f;
    g_nspawned = 0, g_nreleased = 0;
    AnimateVelRotGeneric(&v, 1);
    CHECK(NF(phys, 0x10, float) == -0.25f * 0.15f && t->mat.pos[1] == 2 * (-0.25f * 0.15f));
    CHECK(v.timeStamp == 0.5f && g_nreleased == 0 && pool->active == t);
    AnimateVelRotGeneric(&v, 1);
    CHECK(g_nspawned == 1 && g_spawned[0] == 44 && g_nreleased == 1 && g_released[0] == &v);
    CHECK(pool->free == t && t->next == free_before && pool->active == NODE(0));
}

/* The collision pass: the target in the car's quadtree leaf (the target groups' side table's entry) is hit through
 * the table's kind (its shape: +0x10) and hit function. */
static void test_collision(void)
{
    uint8_t *tree = WP(HEAP + 0xD00u);
    struct tTarget *t = NODE(0);
    static struct pc_target_group side[4];

    memset(tree, 0, 28);
    NF(tree, 3, uint8_t) = 0;                         /* a leaf */
    NF(tree, 4, float) = -1000.0f, NF(tree, 8, float) = 1000.0f, NF(tree, 12, float) = -1000.0f;
    NF(tree, 16, float) = 1000.0f, NF(tree, 20, uint16_t) = 1, NF(tree, 22, uint16_t) = 2;
    gTargetGroups = tree;
    side[1].target = NODE(5), side[2].target = t, side[3].target = NODE(6);   /* only entry 2 is the leaf's */
    gTargetGroupList = side;
    t->Flags = 2 | 8;
    NODE(5)->Flags = NODE(6)->Flags = 0;
    for (int i = 0; i < 6; i++)
        gTargetCollFuncPtrs[i] = NULL;
    gTargetCollFuncPtrs[1] = hit_fn;                           /* the cone's kind */
    gRace = 0, gGameState = 0, g_hit_calls = 0, g_started = NULL;
    CheckTargetCollision(1);
    CHECK(g_hit_calls == 1 && g_started == t && (t->Flags & 4) && t->slot == 1);
}

static void scene_cars(struct tTarget *cam, uint8_t *sc);

/* A path target (a scripted camera): its path record (a heap record, the target's data) and its path (a PTHD
 * record, through the record's 32-bit field). */
static void test_camera(void)
{
    struct tTarget *cam = NODE(1);
    uint8_t *ctl = gTargetStructPtrs[4], *sc = WP(SCENES + 36), *k = WP(KEYS);
    struct pc_visual *v;
    float out[3];
    int16_t player = 0;
    uint8_t *polys = WP(HEAP + 0xE00u);

    /* the debris test freed node 1; give the camera back its fields */
    cam->infoIdx = CAMERA, cam->data = ctl, cam->Flags = 0;
    numCollisionTargetPtrs = 0, gRace = 0, gEnvList = NULL, g_nvisual = 0;
    StartPathTarget(cam);
    v = &g_visuals[0];
    CHECK(numCollisionTargetPtrs == 1 && CollisionTargetPtrs[0] == cam && cam->sndState == 2 && (cam->Flags & 1));
    CHECK(NF(ctl, 0x14, float) == 20.0f && NF(ctl, 0xC, int16_t) == 1 && NF(ctl, 0xE, uint16_t) == 4);
    CHECK(cam->mat.pos[0] == 101.0f && cam->mat.pos[1] == 5.0f && NF(ctl, 0x18, float) == 101.0f);
    CHECK((NF(k + 0x44, 0x40, uint32_t) & 0x1000) && cam->frames == 4);
    CHECK(gEnvList == v && v->func == camera_visual_fn && v->data == (uintptr_t)cam && v->timeStamp == 0.25f);

    NF(ctl, 0x10, float) = 2.0f;
    GetTargetVelocity(4, out);
    CHECK(out[0] == 2.0f && out[1] == 4.0f && out[2] == 8.0f);    /* key 1's direction * f10 */

    memset(polys, 0, 4 * 24);
    NF(polys + 24, 2, uint16_t) = 0x20 | (4 << 11);               /* a camera trigger of scene 4 */
    stree_surf = polys;
    memset(MODEL(0), 0, sizeof(struct pc_model));
    NF(MODEL(0), MODEL_OFS(0x6C4), int16_t) = -1;
    NF(MODEL(0), MODEL_OFS(0x5A0), uint16_t) = 1;
    TargetCollTrigger(cam, &player, 0, 0);
    CHECK((NF(sc, 0x10, uint32_t) & (1u << 24)) && (NF(sc, 0x10, uint32_t) & 0x200));
    scene_cars(cam, sc);
}

/* A race of 12 cars (docs/port/MORECARS.md 3.1): the scene's flags have bits 24-31 for cars 0-7, as on the N64; car 8
 * and up keep theirs in their car record (PC_CAR_SCENES: bit 4 for this scene, whose trigger is collision index 4),
 * none of the scene's other bits, and count when the trigger asks whether a car is in it already (0x200 is set when
 * the first one comes in). In a race of 6, a car past them is not of it. */
static void car_on(int16_t car, int on)
{
    memset(MODEL(car), 0, sizeof(struct pc_model));
    NF(MODEL(car), MODEL_OFS(0x6C4), int16_t) = -1;
    NF(MODEL(car), MODEL_OFS(0x5A0), uint16_t) = on ? 1 : 0;     /* a wheel on polygon 1, the trigger, or none */
}

static void scene_cars(struct tTarget *cam, uint8_t *sc)
{
    uint32_t *flags = (uint32_t *)(sc + 0x10);
    int16_t p0 = 0, p7 = 7, p8 = 8, p11 = 11;

    gNumCars = 12;
    PC_CAR_SCENES(8) = PC_CAR_SCENES(11) = 0;
    car_on(8, 1);
    TargetCollTrigger(cam, &p8, 0, 0);
    CHECK(PC_CAR_SCENES(8) == 1u << 4 && (*flags & 0xFF00000Fu) == 1u << 24 && (*flags & 0x220) == 0x220);
    car_on(7, 1);
    TargetCollTrigger(cam, &p7, 0, 0);
    CHECK((*flags & 0xFF000000u) == (1u << 24 | 1u << 31));
    car_on(7, 0);
    TargetCollTrigger(cam, &p7, 0, 0);
    car_on(0, 0);
    TargetCollTrigger(cam, &p0, 0, 0);
    CHECK((*flags & 0xFF00000Fu) == 0 && PC_CAR_SCENES(8) == 1u << 4);
    *flags &= ~0x200u;
    car_on(0, 1);                                      /* car 8 is in: car 0 is not the first */
    TargetCollTrigger(cam, &p0, 0, 0);
    CHECK((*flags & 1u << 24) && !(*flags & 0x200));
    car_on(11, 1);
    TargetCollTrigger(cam, &p11, 0, 0);
    CHECK(PC_CAR_SCENES(11) == 1u << 4);
    car_on(8, 0), car_on(11, 0), car_on(0, 0);         /* all out */
    TargetCollTrigger(cam, &p8, 0, 0);
    TargetCollTrigger(cam, &p11, 0, 0);
    TargetCollTrigger(cam, &p0, 0, 0);
    CHECK(PC_CAR_SCENES(8) == 0 && PC_CAR_SCENES(11) == 0 && (*flags & 0xFF00000Fu) == 0);
    car_on(0, 1);                                      /* the first in */
    TargetCollTrigger(cam, &p0, 0, 0);
    CHECK((*flags & 1u << 24) && (*flags & 0x200));

    /* a race of 6: car 8's bit, left from a race of 12, is not of it */
    PC_CAR_SCENES(8) = 1u << 4, gNumCars = 6;
    car_on(0, 0);
    TargetCollTrigger(cam, &p0, 0, 0);
    *flags &= ~0x200u;
    car_on(0, 1);
    TargetCollTrigger(cam, &p0, 0, 0);
    CHECK((*flags & 1u << 24) && (*flags & 0x200));
    PC_CAR_SCENES(8) = 0;
}

/* A bounding box model's collision: the model's box is a row of BBoxCollInfo (row 2, its kind's collVal), read by
 * the hit test (TargetCollBBox) and by the response (setMiniFBCollisionForce), which read the model's orientation,
 * position and velocity after it. */
static void test_reckon(void)
{
    struct tTarget *obj = NODE(1);
    uint8_t *body = TEST_AT(BBoxCollInfo) + 2 * 32, *rec = gBBoxModelStructs, *m = MODEL(0);
    int16_t index = 0;
    float dir[3] = {0, 0, 0}, pos[3] = {0, 0, 0};

    memset(body, 0, 32);
    NF(body, 0, float) = -10.0f, NF(body, 4, float) = 10.0f, NF(body, 8, float) = 10.0f;
    NF(body, 12, float) = -10.0f, NF(body, 16, float) = 10.0f, NF(body, 20, float) = -10.0f;
    NF(body, 24, float) = 5.0f, NF(body, 28, float) = 4.0f;   /* radius, weight */
    memcpy(rec + 4, unitmatrix, sizeof unitmatrix);
    memset(rec + 0x28, 0, 24);
    obj->owner = 0;                                            /* its model: gBBoxModelStructs[0] */
    CHECK(NF(rec, 0, int32_t) == 2);
    memset(m, 0, sizeof(struct pc_model));
    NF(m, MODEL_OFS(0x7EA), int8_t) = 1, NF(m, MODEL_OFS(0x654), float) = 3.0f, NF(m, MODEL_OFS(0x640), int8_t) = 1;
    memcpy(m + MODEL_OFS(0x798), unitmatrix, sizeof unitmatrix);
    memcpy(m + MODEL_OFS(0x7A0), unitmatrix, sizeof unitmatrix);
    /* The car is inside the record's box and already pushed (+0x640): setMiniFBCollisionForce hands it to MiniForceApart. */
    g_hit_other = NULL;
    TargetCollBBox(obj, &index, NULL, 0);
    CHECK(g_hit_other == m);
    /* Past the box's far side (z 12 > 10) and closing at 1 along z: a push of 2500 x the mass ratio (4 / 2). */
    NF(m, MODEL_OFS(0x79C), float) = 12.0f;
    NF(rec, 0x3C, float) = -1.0f;                              /* the record's velocity z */
    g_hit_other = NULL;
    setMiniFBCollisionForce(0, m, rec, dir, pos);
    CHECK(g_hit_other == NULL);
    CHECK(NF(m, MODEL_OFS(0x124), float) == 0.0f && NF(m, MODEL_OFS(0x128), float) == 0.0f &&
          NF(m, MODEL_OFS(0x12C), float) == -5000.0f);
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
