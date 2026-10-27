/* port/assets/tests/test_casts.c: game code that held host pointers in 32-bit integers, run on host pointers
 * (docs/port/CASTS.md): the blast matrix table camera_smooth_lerp fills, the environment visual walk
 * PhysicsObjectList_Update, the camera matrix particles_spawn_emitter reads from a window address, and the
 * high-score name table gTotalNames with each player's name slot D_801424F0, whose slots hold a name index or a
 * profile handle (func_800C9480, func_800F1D04, audio_channel_priority and its caller menu_confirm_dialog,
 * func_800F2888), the ghost allocator _NewGhost (func_800C7200), and the RUSH_CHECK_CASTS check of 32-bit N64
 * address stores (PC_N64W). Host storage here is above 4 GB, as the game's own storage is; the camera matrix part
 * maps the RDRAM window and is skipped if it can't.
 * game: src/blob/camera_smooth_lerp.c src/blob/PhysicsObjectList_Update.c src/blob/particles_spawn_emitter.c
 * game: src/blob/func_800C9480.c src/blob/func_800F1D04.c src/blob/audio_channel_priority.c
 * game: src/blob/menu_confirm_dialog.c src/blob/groups/func_800F2888_grp/func_800F2888.c
 * game: src/blob/groups/menu_save_options/ctx_newghost.c
 */
#include <stddef.h>
#include <string.h>

#define RUSH_CHECK_CASTS   /* this file's own PC_N64W checks, whatever the build's option */
#include "pc_prelude.h"
#include "support.h"

#define ABS(name, addr) __asm__(".globl " #name "\n.set " #name ", " #addr)
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
#define HIGH(p) ((uintptr_t)(p) >> 32 != 0)   /* a host address a u32 can't hold */

/* ---------------------------------------------------------------- the blast matrices (camera_smooth_lerp) */

void camera_smooth_lerp(void);

void *D_8013C238[50];                      /* gBlastMats: blast.c's tBlastMat, a matrix pointer an entry */
int16_t D_8013C094;
int32_t D_80154FD8[6 * 15];                /* the blast rings, 0x3C bytes each */
int32_t D_80154660[6 * 100];               /* the blasts, 0x190 bytes each */
static float blast_mats[50][12];
static int blast_next;

void *func_800A7D6C(void) { return blast_mats[blast_next++]; }

static void test_blast_mats(void)
{
    int i;

    CHECK(HIGH(blast_mats));
    memset(D_8013C238, 0, sizeof D_8013C238);
    D_8013C094 = 7;
    blast_next = 0;
    camera_smooth_lerp();
    CHECK(blast_next == 50);
    for (i = 0; i < 50; i++)
        CHECK(D_8013C238[i] == blast_mats[i]);
    for (i = 0; i < 6; i++) {
        CHECK(D_80154FD8[i * 15] == -1);
        CHECK(D_80154660[i * 100] == -1 && D_80154660[i * 100 + 0x150 / 4] == -1);
    }
    CHECK(D_8013C094 == 0);
}

/* ---------------------------------------------------------------- the environment visuals (PhysicsObjectList_Update) */

void PhysicsObjectList_Update(void);

void *D_801391F0;                          /* the active environment visuals, linked through next */
static int voice_set_calls;
void audio_voice_set(void) { voice_set_calls++; }

static struct pc_visual visuals[4];
static struct { void *v; int arg; } calls[8];
static int ncalls;

static void record(void *v, short arg)
{
    if (ncalls < 8)
        calls[ncalls].v = v, calls[ncalls].arg = arg;
    ncalls++;
}

/* A visual whose animator ends it: it unlinks itself, as entity_transform_apply does from the head. */
static void end_self(void *v, short arg)
{
    record(v, arg);
    if (D_801391F0 == v)
        D_801391F0 = ((struct pc_visual *)v)->next;
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
    D_801391F0 = &visuals[0];
    ncalls = voice_set_calls = 0;
    PhysicsObjectList_Update();
    CHECK(ncalls == 3);
    CHECK(calls[0].v == &visuals[0] && calls[0].arg == 1);
    CHECK(calls[1].v == &visuals[1] && calls[1].arg == 1);
    CHECK(calls[2].v == &visuals[3] && calls[2].arg == 1);
    CHECK(D_801391F0 == &visuals[1]);
    CHECK(voice_set_calls == 1);
}

/* ---------------------------------------------------------------- the camera matrix (particles_spawn_emitter) */

void particles_spawn_emitter(int32_t arg0);

ABS(D_80124FD0, 0x80124FD0);               /* each window's matrix, a u32 window address (game arena) */
ABS(gCamera, 0x80150B70);                  /* Camera[], 0x98 bytes each (game arena) */
#define MTX_AT 0x80300000u                 /* a matrix of func_800A7D6C's pool, in the window */

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
    particles_spawn_emitter(2);
    CHECK(mtx[0x24 / 4] == 3.5f);
    CHECK(mtx[0x2C / 4] == -2.0f);
    ((uint32_t *)(uintptr_t)0x80124FD0u)[3] = 0;   /* no matrix: nothing happens */
    particles_spawn_emitter(3);
}

/* ---------------------------------------------------------------- the high-score name table */

void func_800C9480(void);
int16_t func_800F1D04(uint8_t *name);
void audio_channel_priority(intptr_t handle);
int32_t menu_confirm_dialog(void **slot);
void func_800F2888(void);

intptr_t gTotalNames[12][3][5];            /* a name slot (0..19), -1, or a profile handle */
intptr_t D_801424F0[4];
struct weights { uint8_t prefix[4]; float weight[3][5]; uint8_t suffix[32]; } D_80150F88[12];
/* The cached names, D_80151968[20][13], with known bytes before them: a "no name" test that misses reads [-1]. */
struct { char before[13]; uint8_t names[20][13]; } name_block = { "BEFORE", { { 0 } } };
ALIAS(D_80151968, name_block, 13);
#define D_80151968 name_block.names
uint16_t D_80151A78[20];                   /* their ages */
int16_t active_player_count;
int32_t D_801148D4[8];                     /* each pad's name slot */
char D_80143A50[4][13];                    /* each player's name */
int8_t D_80143F18[4];
int16_t D_80144010[4], D_80144C40[4];
int8_t D_80151AC0[3][5];                   /* which player holds each place */
/* The input records, as func_800F2888 sees them: the active pad at +1, the profile handle at +0x48 (80 bytes a
 * record on the PC). One player is enough here. */
struct input { uint8_t pad00, active, pad02[0x46]; void *profile; } input_rec0[1];
struct profile { uint8_t pad00[8]; int32_t valid; uint8_t pad0C[8]; char name[13]; };

static struct profile profile_data[2];
static struct profile *profiles[2] = { &profile_data[0], &profile_data[1] };   /* D_80146150-like handles */

void *n64_memset(void *d, int c, unsigned int n) { return memset(d, c, n); }
unsigned int n64_strlen(const char *s) { return (unsigned int)strlen(s); }
int32_t func_8008AD04(uint8_t *a, uint8_t *b) { return strcmp((char *)a, (char *)b); }
uint8_t *func_800A473C(uint8_t *d, uint8_t *s) { return (uint8_t *)strcpy((char *)d, (char *)s); }
void draw_minimap(void) {}

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
    memset(D_80151968, 'x', sizeof D_80151968);
    func_800C9480();
    CHECK(slots_equal(-1) == 12 * 15);
    CHECK(D_80151968[19][12] == 0 && D_80151A78[19] == 0);
}

/* audio_channel_priority removes one handle from every row; a value with the same low 32 bits stays. */
static void test_remove_handle(void)
{
    intptr_t h = (intptr_t)&profiles[0], alias = h ^ ((intptr_t)1 << 40);
    float *wt = D_80150F88[3].weight[2];
    int i;

    CHECK(HIGH(h));
    func_800C9480();
    gTotalNames[3][2][0] = 4;
    gTotalNames[3][2][1] = h;
    gTotalNames[3][2][2] = 7;
    gTotalNames[3][2][3] = h;
    gTotalNames[3][2][4] = alias;
    gTotalNames[11][0][4] = h;
    for (i = 0; i < 5; i++)
        wt[i] = 10.0f + i;
    audio_channel_priority(h);
    CHECK(gTotalNames[3][2][0] == 4 && gTotalNames[3][2][1] == 7 && gTotalNames[3][2][2] == alias);
    CHECK(gTotalNames[3][2][3] == -1 && gTotalNames[3][2][4] == -1);
    CHECK(wt[0] == 10.0f && wt[1] == 12.0f && wt[2] == 14.0f && wt[3] == 0.0f && wt[4] == 0.0f);
    CHECK(gTotalNames[11][0][4] == -1);
    CHECK(slots_equal(h) == 0 && slots_equal(alias) == 1);
}

/* Clearing a save slot (menu_confirm_dialog) takes its profile out of the table with the whole handle. (An s32
 * argument there fails on x86-64, whose 32-bit moves clear the upper half; aarch64 leaves it in the register.) */
static uint8_t save_data[0x800], file_block[0x50];
static uint8_t *save_handle = save_data;
static void *file_handle = file_block;
/* The profile's slot record as menu_confirm_dialog reads it: its pak file handle at +8, SaveSlot.data at 48. */
static struct { uint8_t pad0[8]; void **file; uint8_t pad16[32]; uint8_t **data; } slot_record = {
    { 0 }, &file_handle, { 0 }, &save_handle };
static void *slot_ptr = &slot_record;
int32_t func_800CCEFC(void **slot) { return 1; }
void PS_WriteFile(void **file, uintptr_t data, int32_t size) {}
int8_t PS_FlushFile(void **file, int32_t flags) { return 1; }
int32_t PS_FlushPak(int32_t pak, int32_t flags) { return 1; }
int32_t object_render_cleanup(void **slot) { return 0; }

static void test_clear_slot(void)
{
    intptr_t h = (intptr_t)&slot_ptr;

    CHECK(HIGH(h));
    func_800C9480();
    gTotalNames[0][1][2] = h;
    gTotalNames[0][1][3] = h ^ ((intptr_t)1 << 36);
    memset(save_data, 0x55, sizeof save_data);
    CHECK(menu_confirm_dialog((void **)h) == 1);
    CHECK(save_data[0x8C] == 0 && save_data[0x6F4 + 0x4B] == 0 && save_data[0x8B] == 0x55);
    CHECK(slots_equal(h) == 0 && gTotalNames[0][1][2] == (h ^ ((intptr_t)1 << 36)));
}

/* func_800F1D04 counts only name slots as references: a handle whose low 32 bits look like slot 19 is none. */
static void test_name_slots(void)
{
    int i;
    int16_t got;

    func_800C9480();
    for (i = 0; i < 20; i++) {
        snprintf((char *)D_80151968[i], 13, "NAME%02d", i);
        D_80151A78[i] = (uint16_t)(i == 5 ? 100 : 1);   /* slot 5 is the oldest */
    }
    for (i = 0; i < 19; i++)
        gTotalNames[i % 12][i / 12][i % 5] = i;
    gTotalNames[7][2][4] = (intptr_t)0x100000013;   /* not slot 19 */
    got = func_800F1D04((uint8_t *)"NEWBIE");
    CHECK(got == 19);
    CHECK(strcmp((char *)D_80151968[19], "NEWBIE") == 0 && D_80151A78[19] == 0);
    CHECK(gTotalNames[7][2][4] == (intptr_t)0x100000013 && gTotalNames[5][0][0] == 5);
}

/* func_800F2888: a saved profile's handle goes into D_801424F0 whole; no name (-1) clears the player's name. */
static void test_player_names(void)
{
    active_player_count = 1;
    memset(D_80151AC0, -1, sizeof D_80151AC0);
    D_80151AC0[1][2] = 0;                    /* player 0 holds a place */
    input_rec0[0].active = 3;
    input_rec0[0].profile = &profiles[1];
    profile_data[1].valid = 1;
    strcpy(profile_data[1].name, "SAVED");
    D_80143F18[0] = 0;
    func_800F2888();
    CHECK(D_801424F0[0] == (intptr_t)&profiles[1] && HIGH(D_801424F0[0]));
    CHECK(D_80143F18[0] == 0 && strcmp(D_80143A50[0], "SAVED") == 0);

    profile_data[1].valid = 0;               /* a default profile: its pad's name slot, none here */
    D_801148D4[3] = -1;
    memset(D_80143A50[0], 'z', 12);
    D_80143A50[0][12] = 0;
    D_80143F18[0] = 0;
    D_80144C40[0] = 9;
    func_800F2888();
    CHECK(D_801424F0[0] == -1);
    CHECK(D_80143F18[0] == 1 && D_80144010[0] == -1);
    CHECK(D_80143A50[0][0] == 0 && D_80144C40[0] == 0);

    D_801148D4[3] = 6;                       /* its pad's cached name */
    strcpy((char *)D_80151968[6], "PADSIX");
    func_800F2888();
    CHECK(D_801424F0[0] == 6 && strcmp(D_80143A50[0], "PADSIX") == 0 && D_80144C40[0] == 6);
}

/* ---------------------------------------------------------------- the ghosts (_NewGhost) */

void **func_800C7200(void);

/* menu_save_options' Ghost on the PC (64 bytes), and what the rest of ctx_newghost.c references. */
struct ghost { void *ll; void **fileh; int8_t track; uint8_t backward, name[13], pad17; uint32_t playerId[2];
               float finish; uint8_t **datah; void **header; };
void *AllocateHandleHeap(void *heap, uint32_t size) { return NULL; }
void *audio_buffer_sync(uint32_t address) { return NULL; }
void audio_reverb_update(uint32_t address, int32_t mode) {}
int32_t car_angular_velocity_clamp(void *a, void *b, void *c, int32_t d) { return 0; }
float D_8002AFB4;
float D_80111754[64];
int8_t D_8014978C;
uint8_t D_80152770[64];
uint32_t format_string_parse(void *data, int32_t size) { return 0; }
void *func_800A47C0(void *dst, void *src, uint32_t n) { return memmove(dst, src, n); }
void *n64_memcpy(void *d, const void *s, unsigned int n) { return memcpy(d, s, n); }
int32_t osRecvMesg(void *q, void **m, int32_t flags) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t flags) { return 0; }
void *sound_play_menu(void *heap, uint32_t size) { return NULL; }
ABS(D_8013F1F8, 0x8013F1F8);               /* the N64's handle slots and records, in the game arena: */
ABS(D_8013F390, 0x8013F390);               /* not used on the PC */

static void test_ghosts(void)
{
    void **h[64], **again;
    struct ghost *g;
    int i, j, bad = 0;

    for (i = 0; i < 64; i++) {
        h[i] = func_800C7200();
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
    CHECK(func_800C7200() == NULL);              /* all 64 taken */
    for (i = 0; i < 64 && !bad; i++) {
        g = *h[i];
        for (j = 0; j < i; j++)
            bad += h[j] == h[i] || *h[j] == *h[i];
        bad += g->ll != h[i] || g->header != (void **)&h[i] || g->name[0] != 0xA0 + (i & 15) || g->datah == NULL;
    }
    CHECK(bad == 0);                             /* separate handles, records that don't overlap */
    g = *h[37];
    *h[37] = NULL;                               /* _FreeGhost */
    again = func_800C7200();
    CHECK(again == h[37] && *again == g);
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
    test_check_casts();
    return TEST_DONE();
}
