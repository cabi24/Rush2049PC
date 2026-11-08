/* port/assets/tests/test_casts.c: game code that held host pointers in 32-bit integers, run on host pointers
 * (docs/port/CASTS.md): the blast matrix table InitBlast fills, the environment visual walk
 * UpdateEnvirons, the camera matrix SkyUpdate reads from a window address, and the
 * high-score name table gTotalNames with each player's name slot gThisPlayer, whose slots hold a name index or a
 * profile handle (InitRecords, FindTempNameIdx, RemovePlayerScores and its caller ClearPlayerRecords,
 * RankScores), the ghost allocator _NewGhost (NewGhost), the audio manager's walk of its sound requests
 * (AudioSfxServer), and the RUSH_CHECK_CASTS check of 32-bit N64 address stores (PC_N64W). Host storage here is
 * above 4 GB, as the game's own storage is; the parts with window data (the camera matrix, the sound requests) map
 * the RDRAM window and are skipped if it can't be.
 * game: src/blob/InitBlast.c src/blob/UpdateEnvirons.c src/blob/SkyUpdate.c
 * game: src/blob/InitRecords.c src/blob/FindTempNameIdx.c src/blob/RemovePlayerScores.c
 * game: src/blob/ClearPlayerRecords.c src/blob/groups/highscore_rank/RankScores.c
 * game: src/blob/groups/save/ctx_newghost.c src/blob/groups/audiosfx_play/group.c
 * game: src/blob/groups/listeners/AudUpdateListener.c src/blob/groups/listeners/AudRemoveListener.c
 */
#include <stddef.h>
#include <string.h>

#define RUSH_CHECK_CASTS   /* this file's own PC_N64W checks, whatever the build's option */
#include "pc_prelude.h"
#include "support.h"

#define ABS(name, addr) __asm__(".globl " #name "\n.set " #name ", " #addr)
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
#define HIGH(p) ((uintptr_t)(p) >> 32 != 0)   /* a host address a u32 can't hold */

/* ---------------------------------------------------------------- the blast matrices (InitBlast) */

void InitBlast(void);

void *gBlastMats[50];                      /* gBlastMats: blast.c's tBlastMat, a matrix pointer an entry */
int16_t gBlastMatCnt;
struct pc_blast_ring pc_blast_rings[PC_MAX_LINKS];   /* the blast rings, 0x3C bytes each (data.c's in the game) */
struct pc_blast pc_blasts[PC_MAX_LINKS];             /* the blasts, 0x190 bytes each */
static float blast_mats[50][12];
static int blast_next;

void *ZOID_NewMatrix(void) { return blast_mats[blast_next++]; }

static void test_blast_mats(void)
{
    int i;

    CHECK(HIGH(blast_mats));
    memset(gBlastMats, 0, sizeof gBlastMats);
    gBlastMatCnt = 7;
    blast_next = 0;
    InitBlast();
    CHECK(blast_next == 50);
    for (i = 0; i < 50; i++)
        CHECK(gBlastMats[i] == blast_mats[i]);
    for (i = 0; i < PC_MAX_LINKS; i++) {   /* every slot, not the N64's 6 */
        CHECK(pc_blast_rings[i].ringIdx == -1);
        CHECK(pc_blasts[i].blastObjs[0].mbIdx == -1 && pc_blasts[i].blastObjs[3].mbIdx == -1
              && pc_blasts[i].glowIdx == -1);
    }
    CHECK(gBlastMatCnt == 0);
}

/* ---------------------------------------------------------------- the environment visuals (UpdateEnvirons) */

void UpdateEnvirons(void);

void *gEnvList;                          /* the active environment visuals, linked through next */
static int voice_set_calls;
void AnimateSkids(void) { voice_set_calls++; }

static struct pc_visual visuals[4];
static struct { void *v; int arg; } calls[8];
static int ncalls;

static void record(void *v, short arg)
{
    if (ncalls < 8)
        calls[ncalls].v = v, calls[ncalls].arg = arg;
    ncalls++;
}

/* A visual whose animator ends it: it unlinks itself, as ReleaseEnviron does from the head. */
static void end_self(void *v, short arg)
{
    record(v, arg);
    if (gEnvList == v)
        gEnvList = ((struct pc_visual *)v)->next;
}

static void test_visuals(void)
{
    int i;

    CHECK(HIGH(visuals));
    memset(visuals, 0, sizeof visuals);
    for (i = 0; i < 4; i++) {
        visuals[i].next = i < 3 ? &visuals[i + 1] : NULL;
        visuals[i].timeStamp = 0.0333333f;   /* N64 +0x14, where the 32-bit walk took the func from */
        visuals[i].data = 0x80300000u + i;
    }
    visuals[0].func = end_self;
    visuals[1].func = record;
    visuals[2].func = NULL;                  /* no animator: skipped */
    visuals[3].func = record;
    gEnvList = &visuals[0];
    ncalls = voice_set_calls = 0;
    UpdateEnvirons();
    CHECK(ncalls == 3);
    CHECK(calls[0].v == &visuals[0] && calls[0].arg == 1);
    CHECK(calls[1].v == &visuals[1] && calls[1].arg == 1);
    CHECK(calls[2].v == &visuals[3] && calls[2].arg == 1);
    CHECK(gEnvList == &visuals[1]);
    CHECK(voice_set_calls == 1);
}

/* ---------------------------------------------------------------- the camera matrix (SkyUpdate) */

void SkyUpdate(int32_t arg0);

ABS(skyMatrices, 0x80124FD0);               /* each window's matrix, a u32 window address (game arena) */
ABS(gCamera, 0x80150B70);                  /* Camera[], 0x98 bytes each (game arena) */
#define MTX_AT 0x80300000u                 /* a matrix of ZOID_NewMatrix's pool, in the window */

static void test_camera_matrix(void)
{
    uint8_t *w = test_rdram();
    float *cam, *mtx;

    if (!w) {
        fprintf(stderr, "test_casts: the RDRAM window can't be mapped here: camera matrix part skipped\n");
        return;
    }
    mtx = (float *)(uintptr_t)MTX_AT;
    cam = (float *)(uintptr_t)(0x80150B70u + 2 * 0x98);
    memset(mtx, 0, 0x40);
    ((uint32_t *)(uintptr_t)0x80124FD0u)[2] = MTX_AT;
    cam[0x24 / 4] = 3.5f;
    cam[0x2C / 4] = -2.0f;
    SkyUpdate(2);
    CHECK(mtx[0x24 / 4] == 3.5f);
    CHECK(mtx[0x2C / 4] == -2.0f);
    ((uint32_t *)(uintptr_t)0x80124FD0u)[3] = 0;   /* no matrix: nothing happens */
    SkyUpdate(3);
}

/* ---------------------------------------------------------------- the high-score name table */

void InitRecords(void);
int16_t FindTempNameIdx(uint8_t *name);
void RemovePlayerScores(intptr_t handle);
int32_t ClearPlayerRecords(void **slot);
void RankScores(void);

intptr_t gTotalNames[12][3][5];            /* a name slot (0..19), -1, or a profile handle */
intptr_t gThisPlayer[4];
struct weights { uint8_t prefix[4]; float weight[3][5]; uint8_t suffix[32]; } gTotalRaceStats[12];
/* The cached names, gTempNames[20][13], with known bytes before them: a "no name" test that misses reads [-1]. */
struct { char before[13]; uint8_t names[20][13]; } name_block = { "BEFORE", { { 0 } } };
ALIAS(gTempNames, name_block, 13);
#define gTempNames name_block.names
uint16_t gTempNameAges[20];                   /* their ages */
int16_t gNumPlayers;
int32_t gLastPlayer[8];                     /* each pad's name slot */
char gScoreName[4][13];                    /* each player's name */
int8_t gDrawName[4];
int16_t highscore_gNameSelect[4], highscore_gNameLen[4];
int8_t gNewTotalScore[3][5];                   /* which player holds each place */
/* The input records, as RankScores sees them: the active pad at +1, the profile handle at +0x48 (80 bytes a
 * record on the PC). One player is enough here. */
struct input { uint8_t pad00, active, pad02[0x46]; void *profile; } gPlayers[4];
struct profile { void *next, *prev, *valid; uint8_t pad0C[8]; char name[13]; };   /* PC: valid is the file handle */

static struct profile profile_data[2];
static struct profile *profiles[2] = { &profile_data[0], &profile_data[1] };   /* gJustPlayers-like handles */

void *n64_memset(void *d, int c, unsigned int n) { return memset(d, c, n); }
unsigned int n64_strlen(const char *s) { return (unsigned int)strlen(s); }
int32_t game_strcmp(uint8_t *a, uint8_t *b) { return strcmp((char *)a, (char *)b); }
uint8_t *game_strcpy(uint8_t *d, uint8_t *s) { return (uint8_t *)strcpy((char *)d, (char *)s); }
void ClearTotalStats(void) {}

static int slots_equal(intptr_t v)
{
    int i, n = 0;
    for (i = 0; i < 12 * 15; i++)
        n += (&gTotalNames[0][0][0])[i] == v;
    return n;
}

static void test_table_clear(void)
{
    memset(gTotalNames, 0, sizeof gTotalNames);
    memset(gTempNames, 'x', sizeof gTempNames);
    InitRecords();
    CHECK(slots_equal(-1) == 12 * 15);
    CHECK(gTempNames[19][12] == 0 && gTempNameAges[19] == 0);
}

/* RemovePlayerScores removes one handle from every row; a value with the same low 32 bits stays. */
static void test_remove_handle(void)
{
    intptr_t h = (intptr_t)&profiles[0], alias = h ^ ((intptr_t)1 << 40);
    float *wt = gTotalRaceStats[3].weight[2];
    int i;

    CHECK(HIGH(h));
    InitRecords();
    gTotalNames[3][2][0] = 4;
    gTotalNames[3][2][1] = h;
    gTotalNames[3][2][2] = 7;
    gTotalNames[3][2][3] = h;
    gTotalNames[3][2][4] = alias;
    gTotalNames[11][0][4] = h;
    for (i = 0; i < 5; i++)
        wt[i] = 10.0f + i;
    RemovePlayerScores(h);
    CHECK(gTotalNames[3][2][0] == 4 && gTotalNames[3][2][1] == 7 && gTotalNames[3][2][2] == alias);
    CHECK(gTotalNames[3][2][3] == -1 && gTotalNames[3][2][4] == -1);
    CHECK(wt[0] == 10.0f && wt[1] == 12.0f && wt[2] == 14.0f && wt[3] == 0.0f && wt[4] == 0.0f);
    CHECK(gTotalNames[11][0][4] == -1);
    CHECK(slots_equal(h) == 0 && slots_equal(alias) == 1);
}

/* Clearing a save slot (ClearPlayerRecords) takes its profile out of the table with the whole handle. (An s32
 * argument there fails on x86-64, whose 32-bit moves clear the upper half; aarch64 leaves it in the register.) */
static uint8_t save_data[0x800], file_block[0x60];
static uint8_t *save_handle = save_data;
static void *file_handle = file_block;
/* The profile record as ClearPlayerRecords reads it on the PC (pc_prelude.h): its pak file handle at +16,
 * SaveSlot.data at +56. */
static struct { void *next, *prev; void **file; uint8_t pad24[32]; uint8_t **data; } slot_record = {
    NULL, NULL, &file_handle, { 0 }, &save_handle };
static void *slot_ptr = &slot_record;
int32_t ReadPlayerData(void **slot) { return 1; }
void PS_WriteFile(void **file, uintptr_t data, int32_t size) {}
int8_t PS_FlushFile(void **file, int32_t flags) { return 1; }
int32_t PS_FlushPak(int32_t pak, int32_t flags) { return 1; }
int32_t PurgePlayerData(void **slot) { return 0; }

static void test_clear_slot(void)
{
    intptr_t h = (intptr_t)&slot_ptr;

    CHECK(HIGH(h));
    InitRecords();
    gTotalNames[0][1][2] = h;
    gTotalNames[0][1][3] = h ^ ((intptr_t)1 << 36);
    memset(save_data, 0x55, sizeof save_data);
    CHECK(ClearPlayerRecords((void **)h) == 1);
    CHECK(save_data[0x8C] == 0 && save_data[0x6F4 + 0x4B] == 0 && save_data[0x8B] == 0x55);
    CHECK(slots_equal(h) == 0 && gTotalNames[0][1][2] == (h ^ ((intptr_t)1 << 36)));
}

/* FindTempNameIdx counts only name slots as references: a handle whose low 32 bits look like slot 19 is none. */
static void test_name_slots(void)
{
    int i;
    int16_t got;

    InitRecords();
    for (i = 0; i < 20; i++) {
        snprintf((char *)gTempNames[i], 13, "NAME%02d", i);
        gTempNameAges[i] = (uint16_t)(i == 5 ? 100 : 1);   /* slot 5 is the oldest */
    }
    for (i = 0; i < 19; i++)
        gTotalNames[i % 12][i / 12][i % 5] = i;
    gTotalNames[7][2][4] = (intptr_t)0x100000013;   /* not slot 19 */
    got = FindTempNameIdx((uint8_t *)"NEWBIE");
    CHECK(got == 19);
    CHECK(strcmp((char *)gTempNames[19], "NEWBIE") == 0 && gTempNameAges[19] == 0);
    CHECK(gTotalNames[7][2][4] == (intptr_t)0x100000013 && gTotalNames[5][0][0] == 5);
}

/* RankScores: a saved profile's handle goes into gThisPlayer whole; no name (-1) clears the player's name. */
static void test_player_names(void)
{
    gNumPlayers = 1;
    memset(gNewTotalScore, -1, sizeof gNewTotalScore);
    gNewTotalScore[1][2] = 0;                    /* player 0 holds a place */
    gPlayers[0].active = 3;
    gPlayers[0].profile = &profiles[1];
    profile_data[1].valid = &profile_data[0];
    strcpy(profile_data[1].name, "SAVED");
    gDrawName[0] = 0;
    RankScores();
    CHECK(gThisPlayer[0] == (intptr_t)&profiles[1] && HIGH(gThisPlayer[0]));
    CHECK(gDrawName[0] == 0 && strcmp(gScoreName[0], "SAVED") == 0);

    profile_data[1].valid = NULL;            /* a default profile: its pad's name slot, none here */
    gLastPlayer[3] = -1;
    memset(gScoreName[0], 'z', 12);
    gScoreName[0][12] = 0;
    gDrawName[0] = 0;
    highscore_gNameLen[0] = 9;
    RankScores();
    CHECK(gThisPlayer[0] == -1);
    CHECK(gDrawName[0] == 1 && highscore_gNameSelect[0] == -1);
    CHECK(gScoreName[0][0] == 0 && highscore_gNameLen[0] == 0);

    gLastPlayer[3] = 6;                       /* its pad's cached name */
    strcpy((char *)gTempNames[6], "PADSIX");
    RankScores();
    CHECK(gThisPlayer[0] == 6 && strcmp(gScoreName[0], "PADSIX") == 0 && highscore_gNameLen[0] == 6);
}

/* ---------------------------------------------------------------- the ghosts (_NewGhost) */

void **NewGhost(void);

/* SaveInit' Ghost on the PC (64 bytes), and what the rest of ctx_newghost.c references. */
struct ghost { void *ll; void **fileh; int8_t track; uint8_t backward, name[13], pad17; uint32_t playerId[2];
               float finish; uint8_t **datah; void **header; };
void *AllocateHandleHeap(void *heap, uint32_t size) { return NULL; }
void *GetPtrSize(uint32_t address) { return NULL; }
void DeallocateMemoryInternal(uint32_t address, int32_t mode) {}
int32_t zip(void *a, void *b, void *c, int32_t d) { return 0; }
float gRetraceRate;
float ghostRaceTimes[64];
int8_t gTrackNum;
uint8_t heap_lock_queue[64];
uint32_t CalcChecksumInternal(void *data, int32_t size) { return 0; }
void *game_memmove(void *dst, void *src, uint32_t n) { return memmove(dst, src, n); }
void *n64_memcpy(void *d, const void *s, unsigned int n) { return memcpy(d, s, n); }
int32_t osRecvMesg(void *q, void **m, int32_t flags) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t flags) { return 0; }
void *AllocateMemoryHeapHigh(void *heap, uint32_t size) { return NULL; }
ABS(ghost_file_handles, 0x8013F1F8);               /* the N64's handle slots and records, in the game arena: */
ABS(ghost_file_records, 0x8013F390);               /* not used on the PC */

static void test_ghosts(void)
{
    void **h[64], **again;
    struct ghost *g;
    int i, j, bad = 0;

    for (i = 0; i < 64; i++) {
        h[i] = NewGhost();
        if (!h[i] || !*h[i] || !HIGH(h[i])) {
            bad++;
            continue;
        }
        g = *h[i];
        memset(g, 0xA0 + (i & 15), sizeof *g);   /* a whole PC ghost */
        g->ll = h[i];
        g->header = (void **)&h[i];
    }
    CHECK(bad == 0);
    CHECK(NewGhost() == NULL);              /* all 64 taken */
    for (i = 0; i < 64 && !bad; i++) {
        g = *h[i];
        for (j = 0; j < i; j++)
            bad += h[j] == h[i] || *h[j] == *h[i];
        bad += g->ll != h[i] || g->header != (void **)&h[i] || g->name[0] != 0xA0 + (i & 15) || g->datah == NULL;
    }
    CHECK(bad == 0);                             /* separate handles, records that don't overlap */
    g = *h[37];
    *h[37] = NULL;                               /* _FreeGhost */
    again = NewGhost();
    CHECK(again == h[37] && *again == g);
}

/* ---------------------------------------------------------------- the sound requests (AudioSfxServer) */

void AudioSfxServer(void);

/* The audio manager's request entries (24 bytes, no pointers) are a heap block: window addresses. */
void *gSfxInstances;
int32_t gNumInstances, gNumChannels;
int8_t audiosfx_gInited, gStopAllEffects;
int32_t sfx_history[5];                    /* gStoppedChannels_tail..gStoppedChannels_newest, and the word before */
ALIAS(gStoppedChannels_tail, sfx_history, 4);
ALIAS(gStoppedChannels_newest, sfx_history, 16);
#define gStoppedChannels_newest sfx_history[4]
struct { uint32_t pad[2]; void *tail, *head; } gFreeQ, gCmdQ;   /* the request lists (PcSfxList) */
static int sfx_calls;
int32_t sound_check(int32_t voice) { return voice == 100 ? -1 : 0; }   /* 100 has finished */
void AudioStopEffect(int32_t voice) { sfx_calls++; }
/* What the MusyX calls got: the sfx item started, and each volume, pan, front-back pan and pitch as (voice, value). */
static struct { int32_t item, voice[4], value[4], n[4]; } sfx_got;
static void sfx_set(int k, uint32_t voice, uint32_t value)
{
    sfx_got.voice[k] = (int32_t)voice, sfx_got.value[k] = (int32_t)value, sfx_got.n[k]++;
}
int32_t sound_key_off() { return sfx_calls++, 0; }
int32_t sound_set_volume(uint32_t voice, uint8_t value) { sfx_set(0, voice, value); return sfx_calls++, 0; }
int32_t sound_set_pan(uint32_t voice, uint8_t value) { sfx_set(1, voice, value); return sfx_calls++, 0; }
int32_t sound_set_span(uint32_t voice, uint8_t value) { sfx_set(2, voice, value); return sfx_calls++, 0; }
int32_t sound_set_doppler(uint32_t voice, uint16_t value) { sfx_set(3, voice, value); return sfx_calls++, 0; }
#define SFX_VOICE 0x4401   /* a made-up MusyX voice id */
int32_t sound_start_sfx(uint16_t item, uint8_t channel, uint8_t value)
{
    sfx_got.item = item;
    return sfx_calls++, SFX_VOICE;
}
void sound_set_mono() { sfx_calls++; }
static void *list_last;   /* the list the last link or unlink was on */
static int list_links, list_unlinks;
void InsertLLE(void *list) { list_last = list, list_links++, sfx_calls++; }
void RemoveLLE(void *list) { list_last = list, list_unlinks++, sfx_calls++; }
#define SFX_AT 0x80310000u

static void test_sound_requests(void)
{
    uint8_t *e = (uint8_t *)(uintptr_t)SFX_AT;

    if (!test_rdram()) {
        fprintf(stderr, "test_casts: the RDRAM window can't be mapped here: sound request part skipped\n");
        return;
    }
    memset(e, 0, 3 * 24);
    e[0] = e[1] = 1, *(int32_t *)(e + 8) = 7, *(int32_t *)(e + 0x10) = 100;            /* finished */
    e[24] = e[25] = 1, *(int32_t *)(e + 32) = 8, *(int32_t *)(e + 24 + 0x10) = 101;    /* still playing */
    gSfxInstances = e;
    gNumInstances = 3;
    audiosfx_gInited = 1;
    gStopAllEffects = 0;
    gStoppedChannels_newest = 0;
    sfx_calls = 0;
    AudioSfxServer();
    CHECK(e[0] == 0 && *(int32_t *)(e + 8) == 0);   /* retired in place */
    CHECK(e[24] == 1 && *(int32_t *)(e + 32) == 8);
    CHECK(gStoppedChannels_newest == 1 && sfx_calls == 0);
}

/* A request to start an effect, then one to change it (docs/port/AUDIO.md 9): the effect started is the entry's
 * resource (its low half, which the N64 reads as the u16 at +0xE); the floats become bytes the way IDO converts
 * them (a negative volume is 0xFF); the pitch goes to the effect's voice. Values made up. */
typedef struct { void *prev, *next; uint8_t kind, busy, pad[2]; void *entry; float value, stream, priority, route; }
    TestSfxNode;

static void test_sound_effect(void)
{
    uint8_t *e = (uint8_t *)(uintptr_t)SFX_AT;
    static TestSfxNode start, change;

    if (!test_rdram())
        return;
    memset(e, 0, 24);
    e[0] = 1, *(int32_t *)(e + 8) = 9, *(int32_t *)(e + 12) = 0x123;   /* in use, handle 9, effect 0x123 */
    *(int32_t *)(e + 0x10) = -1;
    memset(&start, 0, sizeof start), memset(&change, 0, sizeof change);
    start.kind = 0, start.entry = e, start.value = 0.5f, start.stream = 0.0f, start.next = &change;
    change.kind = 3, change.entry = e, change.value = -0.25f, change.stream = -2.0f, change.priority = 0.25f;
    change.route = 0.5f;
    gCmdQ.head = &start;
    gSfxInstances = e;
    gNumInstances = 1;
    audiosfx_gInited = 1;
    gStopAllEffects = 0;
    gStoppedChannels_newest = 0;
    gNumChannels = 1;   /* one start allowed this retrace */
    memset(&sfx_got, 0, sizeof sfx_got);
    AudioSfxServer();
    gCmdQ.head = NULL;
    CHECK(sfx_got.item == 0x123);                                   /* not 0, the resource's high half */
    CHECK(*(int32_t *)(e + 0x10) == SFX_VOICE && e[1] == 1);
    CHECK(sfx_got.n[0] == 2 && sfx_got.voice[0] == SFX_VOICE);     /* the start's volume, then the change's */
    CHECK(sfx_got.value[0] == 0xFF);                                /* -0.25 * 127: IDO's u32 is 0xFFFFFFFF */
    CHECK(sfx_got.n[1] == 1 && sfx_got.value[1] == 63);             /* the start's pan, (0 + 1) / 2 * 127 */
    CHECK(sfx_got.n[2] == 1 && sfx_got.value[2] == 79);             /* (0.25 + 1) / 2 * 127 = 79.375 */
    CHECK(sfx_got.n[3] == 1 && sfx_got.voice[3] == SFX_VOICE);     /* the pitch: to the voice, */
    CHECK(sfx_got.value[3] == 4095);                                /* 0.5 * 8192 - 1 */
}

/* The listener packets (docs/port/AUDIO.md 9): UpdateListeners hands AudUpdateListener the car's position and the
 * camera's vectors for its packet, and ProcessEmitters reads them as its listener; AudRemoveListener stops one. The
 * packet (AudAddListener's Packet) has two host-sized links, then a type and a posted byte, then the vectors. */
typedef struct { void *next, *prev; uint8_t type, posted, pad[2]; float a[3], b[3], c[3], d[3]; } TestPacket;
int __unit_dead[8];
uint8_t audio_lock_queue[24];
struct { uint8_t indirect, doubly, pad[2]; uint32_t count; void *head, *tail; } gFreeListeners, gActiveListeners;
void AudUpdateListener(void *node, float *a, float *b, float *c, float *d);
void AudRemoveListener(void *packet);

static void test_listener_packet(void)
{
    static TestPacket p;
    float pos[3] = {123.5f, -7.25f, 900.0f}, b[3] = {1, 2, 3}, right[3] = {0, 0, -1}, fwd[3] = {0.5f, 0, 0.5f};
    void *link = &p;

    p.next = link, p.prev = link, p.type = 0, p.posted = 1;
    AudUpdateListener(&p, pos, b, right, fwd);
    CHECK(p.next == link && p.prev == link && p.type == 0 && p.posted == 1);   /* the links and bytes intact */
    CHECK(p.a[0] == 123.5f && p.a[1] == -7.25f && p.a[2] == 900.0f);         /* the listener's position */
    CHECK(p.b[2] == 3 && p.c[2] == -1 && p.d[0] == 0.5f && p.d[2] == 0.5f);
    list_links = list_unlinks = 0;
    AudRemoveListener(&p);                       /* posted: off the listener list, onto the free list */
    CHECK(list_unlinks == 1 && list_links == 1 && list_last == (void *)&gFreeListeners);
    CHECK(p.posted == 0 && p.type == 1 && p.next == link && p.prev == link);
    list_links = list_unlinks = 0;
    AudRemoveListener(&p);                       /* not posted: only the free list */
    CHECK(list_unlinks == 0 && list_links == 1);
}

/* pc_prelude.h's IDO conversions, on made-up values. */
static void test_ido_f2u(void)
{
    volatile float nan = 0.0f;
    CHECK(pc_mips_f2u(0.0f) == 0 && pc_mips_f2u(-0.0f) == 0 && pc_mips_f2u(-0.75f) == 0);
    CHECK(pc_mips_f2u(127.9f) == 127 && pc_mips_f2u(65536.5f) == 65536);
    CHECK(pc_mips_f2u(-1.0f) == 0xFFFFFFFFu && pc_mips_f2u(-31.75f) == 0xFFFFFFFFu);
    CHECK((uint8_t)pc_mips_f2u(-200.0f) == 0xFF && pc_mips_f2u(-3e9f) == 0xFFFFFFFFu);
    CHECK(pc_mips_f2u(2147483648.0f) == 0x80000000u && pc_mips_f2u(3000000000.0f) == 3000000000u);
    CHECK(pc_mips_f2u(4294967296.0f) == 0xFFFFFFFFu && pc_mips_f2u(1e20f) == 0xFFFFFFFFu);
    CHECK(pc_mips_f2u(nan / nan) == 0xFFFFFFFFu);
    CHECK(pc_mips_d2u(-0.5) == 0 && pc_mips_d2u(-2.5) == 0xFFFFFFFFu && pc_mips_d2u(4294967295.75) == 0xFFFFFFFFu);
    CHECK(pc_mips_d2u(2147483649.5) == 0x80000001u && pc_mips_d2u(1234.99) == 1234);
}

/* ---------------------------------------------------------------- RUSH_CHECK_CASTS */

static struct { const char *file; int line; uintptr_t value; int n; } cast_fail;

void pc_cast_fail(const char *file, int line, uintptr_t value)
{
    cast_fail.file = file, cast_fail.line = line, cast_fail.value = value, cast_fail.n++;
}

static void test_check_casts(void)
{
    int line;

    CHECK(PC_N64W((void *)(uintptr_t)0x80123456u) == 0x80123456u);   /* a window address */
    CHECK(PC_N64W(NULL) == 0 && PC_N64W(0xB0039B40u) == 0xB0039B40u);
    CHECK(cast_fail.n == 0);
    line = __LINE__ + 1;
    CHECK(PC_N64W(&profiles[0]) == (uint32_t)(uintptr_t)&profiles[0]);   /* host storage */
    CHECK(cast_fail.n == 1 && cast_fail.value == (uintptr_t)&profiles[0] && cast_fail.line == line);
    CHECK(cast_fail.file && strstr(cast_fail.file, "test_casts.c"));
}

int main(void)
{
    if (test_rdram())
        pc_fix_absolute_got();
    test_blast_mats();
    test_visuals();
    test_camera_matrix();
    test_table_clear();
    test_remove_handle();
    test_clear_slot();
    test_name_slots();
    test_player_names();
    test_ghosts();
    test_sound_requests();
    test_sound_effect();
    test_ido_f2u();
    test_listener_packet();
    test_check_casts();
    return TEST_DONE();
}
