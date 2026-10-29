/* port/assets/tests/test_save.c: the controller pak's file records and the save records around them on their PC
 * layouts (docs/port/POINTERS.md 9.10), through the game's own save code on a pak of the test's: the pak file record
 * tPS_File (paksys; 96 bytes on the PC) with its file-function table, the profile record (64 bytes), a ghost
 * (64 bytes) and its header (file data, in the N64's 88 bytes), and the heap's handles (8-byte slots).
 * PS_InitPakSys and PS_PakServer find the pak in port 1; CreatePlayer makes a profile, and its save file reaches the
 * pak through PS_WriteFile and PS_FlushFile; taking the pak out removes the profile through RemoveFile (_RemovePak);
 * putting it back reads the profile in (SaveServer, _ReadPlayer, the check of every block, PurgePlayerData);
 * LoadPlayerData (func_800CCEFC) and the car settings write back through SaveFlush; MoveFile moves a profile to
 * another file; CompressGhost makes a ghost, SaveGhost writes it, _ReadGhostHeader and _ReadGhostData read it back,
 * and the replay plays and records through its header; PS_DeleteFile deletes a ghost's and a profile's file through
 * RemoveFile. The pak's notes are the test's (in place of the SI and the Pfs C, which the selftest covers); the
 * game's heap runs in an RDRAM window the test maps (exit 77, skipped, if it can't), so its blocks and handles are
 * window addresses, as in the game.
 * game: src/blob/groups/paksys/group.c nonmatching/game/drone_set_catchup.c src/blob/PS_ReadFile.c
 * game: src/blob/groups/pak_write_cluster/group.c src/blob/SaveFlush.c src/blob/func_800A43FC.c
 * game: src/blob/func_80091FBC.c src/blob/func_8009211C.c src/blob/func_800A150C.c src/blob/func_800A1644.c
 * game: src/blob/groups/menu_save_options/save.c src/blob/groups/menu_save_options/group.c
 * game: src/blob/groups/menu_save_options/ctx_slot.c src/blob/groups/menu_save_options/ctx_newghost.c
 * game: src/blob/groups/save_option/save_option.c src/blob/groups/setting_c7578/candidate.c
 * game: src/blob/groups/filename_cce5c/filename.c src/blob/audio_volume_pan.c src/blob/func_800949D4.c
 * game: src/blob/object_render_cleanup.c src/blob/groups/hash/group.c src/blob/func_8008AD04.c
 * game: src/blob/func_800950AC.c src/blob/func_800A1910.c src/blob/func_800A473C.c src/blob/func_800C77C4.c
 * game: src/blob/groups/audio_heap/group.c src/blob/func_800E5D64.c src/ovl_a/func_80398BF0.c
 * game: src/blob/groups/pfs_status/func_800A1E94.o3.c
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define ABS(name, addr) __asm__(".globl " #name "\n.set " #name ", " #addr)
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
#define W32(p) ((uint32_t)(uintptr_t)(p))
#define IN_WINDOW(p) ((uintptr_t)(p) >= 0x80000000u && (uintptr_t)(p) < 0x80800000u)

/* ---------------------------------------------------------------- the PC layouts */

/* The pak file record (paksys' tPS_File). */
struct psfile {
    void **next, **prev;
    void (*RemoveFile)(void **);
    void (*MoveFile)(void **, void **);
    uint8_t pak, fileNo;
    char name[35], ext[11];
    uint32_t size, blocks;
    uint8_t **data;
};
_Static_assert(sizeof(struct psfile) == PC_PSFILE_SIZE, "tPS_File's PC size");
_Static_assert(offsetof(struct psfile, RemoveFile) == PSF_OFS(8) && offsetof(struct psfile, MoveFile) == PSF_OFS(12),
               "the file functions");
_Static_assert(offsetof(struct psfile, pak) == PSF_OFS(16) && offsetof(struct psfile, name) == PSF_OFS(18) &&
               offsetof(struct psfile, ext) == PSF_OFS(53) && offsetof(struct psfile, size) == PSF_OFS(64) &&
               offsetof(struct psfile, data) == PSF_OFS(72) && PSF_OFS(76) == PC_PSFILE_SIZE, "PSF_OFS");
/* The profile record (save.c's SaveSlot, save_option.c's tPlayerProfile). */
struct profile {
    void **next, **prev;
    void **fileh;
    uint32_t id[2];
    char name[13];
    int8_t controls[11];
    uint8_t **data;
};
_Static_assert(sizeof(struct profile) == PC_PROFILE_SIZE && PROFILE_OFS(48) == PC_PROFILE_SIZE, "the profile's size");
_Static_assert(offsetof(struct profile, fileh) == PROFILE_OFS(8) && offsetof(struct profile, id) == PROFILE_OFS(12) &&
               offsetof(struct profile, name) == PROFILE_OFS(20) && offsetof(struct profile, controls) ==
               PROFILE_OFS(33) && offsetof(struct profile, data) == PROFILE_OFS(44), "PROFILE_OFS");
/* A ghost (menu_save_options' Ghost). */
struct ghost {
    void **ll;
    void **fileh;
    int8_t track;
    uint8_t backward;
    char name[13];
    uint8_t pad17;
    uint32_t playerId[2];
    float finish;
    uint8_t **datah;
    uint8_t **header;
};
_Static_assert(sizeof(struct ghost) == PC_GHOST_SIZE && GHOST_OFS(0x2C) == PC_GHOST_SIZE &&
               offsetof(struct ghost, fileh) == GHOST_OFS(4) && offsetof(struct ghost, finish) == GHOST_OFS(0x20) &&
               offsetof(struct ghost, datah) == GHOST_OFS(0x24) && offsetof(struct ghost, header) == GHOST_OFS(0x28),
               "GHOST_OFS");
/* The ghost header is file data and keeps its N64 layout: start, the sample buffer, is a 32-bit window address. */
enum { GH_SUM = 0, GH_REV = 4, GH_FLAG = 5, GH_TRACK = 6, GH_BACK = 7, GH_NAME = 0x16, GH_ID = 0x28, GH_TIME = 0x34,
       GH_FINISH = 0x38, GH_ZSIZE = 0x3C, GH_SIZE = 0x40, GH_ZSUM = 0x44, GH_USUM = 0x48, GH_START = 0x4C,
       GH_OFFSET = 0x50, GH_COUNT = 0x54, GH_BYTES = 0x58 };
struct dllist { uint8_t indirect, doubly, pad[2]; uint32_t count; void **head, **tail; };
typedef struct {   /* libultra's OSPfs, host layout */
    int32_t status;
    void *queue;
    int32_t channel;
    uint8_t id[32], label[32];
    int32_t version, dir_size, inode_table, minode_table, dir_table, inode_start_page;
    uint8_t banks, activebank;
} OSPfs;
typedef struct {
    int8_t flush, flag1, flag2, opaque3;
    uint16_t mapBytes, opaque6;
    uint32_t file_size, game_code;
    uint16_t company_code;
    uint8_t ext_name[4], game_name[16];
} PakFile;
typedef struct {   /* paksys' PakRecord (docs/port/POINTERS.md 9.6) */
    uint8_t changed;
    int8_t inserted, opaque2, opaque3, opaque4, damaged, opaque6, flushing, flushed, opaque9, opaque10, flush;
    OSPfs pfs;
    int32_t error, FreeSpace;
    uint8_t motorOn, request, level, opaque127;
    int16_t phase;
    uint8_t opaque130[2];
    PakFile files[16];
} PakRecord;
_Static_assert(sizeof(PakRecord) == 0x310, "the controller/pak record");

#define PAK 1                                   /* the port the pak is in: not 0, so strides count */
#define FILE_OF(h) ((struct psfile *)*(void **)(h))
#define PROFILE_OF(h) ((struct profile *)*(void **)(h))
#define GHOST_OF(h) ((struct ghost *)*(void **)(h))
#define U32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))
#define F32(p, o) (*(float *)((uint8_t *)(p) + (o)))
#define MODEL_F(o) (*(float *)&D_8014A250[0][o])

/* ---------------------------------------------------------------- the game's code under test */

void car_lod_select(void (*error)(int32_t, int32_t, uint32_t, int32_t, int8_t *, int8_t *, void *));   /* PS_InitPakSys */
void SaveServer(int32_t forceUpdate, int32_t rumbleOnly);
void menu_save_options(void);
void **CreatePlayer(int32_t pak, uint8_t *name);
int32_t func_800CCEFC(void **player);                           /* LoadPlayerData */
void func_800C7578(void **player, uint8_t car, uint8_t field, int8_t value);   /* a car setting */
void pak_delete_file_retry(void **fileh);                       /* PS_DeleteFile */
void PS_ReadFile(void **fileh);
void PS_WriteFile(void **fileh, void *data, uint32_t size);
void SaveFlush(int32_t mode);
void func_800A43FC(void);                                       /* the pak insert scan */
void **func_800C7200(void);                                     /* _NewGhost */
void **CompressGhost(void *header, int8_t *blocks);
int32_t SaveGhost(int32_t pak, void **player, void **ghosth, int32_t limbo);
void DeleteGhost(void **ghosth);
int32_t func_800CBF2C(void **ghosth, int32_t decode, int32_t select);   /* _ReadGhostData */
int32_t func_800E5D64(int32_t player, float *frameTime);        /* the ghost replay */
void **func_80398BF0(int32_t fileNo);                           /* a file of pak D_803B9BBA (overlay A) */
void func_800E7D0C(uint16_t handles, uint16_t more);            /* the heap */
void *AllocateHandleHeap(void *heap, uint32_t size);
void *AllocateMemoryHeap(void *heap, uint32_t size);
uint32_t format_string_parse(uint8_t *data, uint32_t size);
void func_800A150C(uint8_t *out, uint8_t *in, uint8_t limit);   /* ASCII -> pak font */
void audio_volume_pan(uintptr_t fileh);                         /* the files' RemoveFile */
void func_800949D4(intptr_t from, intptr_t to);                 /* the files' MoveFile */

/* ---------------------------------------------------------------- the game's data they use */

PakRecord D_80144030[4];
struct dllist D_80144D60[4];                    /* gPS_Files */
ALIAS(D_80144D68, D_80144D60, 8);               /* their heads */
struct dllist D_801460E0;                       /* the free file handles */
void *D_80144C60[64];                           /* the handles */
struct psfile D_80144DC0[64];                   /* the file records */
int8_t D_8011194C, D_8011EAE8 = -1, D_8011EAE0;
uint8_t D_801497D0[32], D_80035458[32], D_80152770[32], D_800349F0[512], D_800329F0[16];
void *D_801527E4, *D_80144008;
struct { int8_t present; uint8_t pad[15]; } gControllerData[4];
struct { uint16_t type; uint8_t status, errno_; } D_80149440[4];
uint16_t D_8011EAEC[256];                       /* the pak font, set up by main */

struct dllist D_8012E6D8, D_80152020;           /* gPlayerFiles (doubly linked), gGhostFiles */
ALIAS(D_8012E6E0, D_8012E6D8, 8);
ALIAS(D_80152028, D_80152020, 8);
struct profile D_8013E5D8[4][16];               /* the profile records */
void *D_8013C128[4][16];                        /* their handles */
struct profile D_801391F8[4], *D_80146150[4];   /* the four default profiles */
void *D_801392F0[4];
uint8_t D_8013A028[4][2064];
char D_801211B0[] = "PLAYER %d";
int8_t D_80156994, D_8013FECA;
void *D_8013FEC4, **D_8012E6F8;
uint8_t D_80117428, D_80117424;
int8_t D_8014978C, D_80146108[64], D_801164A1, D_801148E4;
int16_t D_801164A4[32], D_8013FEC8;
int32_t D_8002E8E8[160];
/* the file names' parts and formats (the test's own) */
char D_801211CC[] = "CHEATS", D_801211F8[9] = "RUSH2049", D_801211DC[] = "P", D_801211E0[] = "P",
     D_80121204[] = "%0*d", D_801211BC[] = "RUSH2049", D_801211C8[] = "P";
char D_801211E4[] = "%s%d%s", D_801211EC[] = "B", D_801211F0[] = "F", D_801211F4[] = "G";
void **D_80152698[4];                           /* gGhosts: the ghosts of a race */
void *D_801527C8;                               /* the heap */
int32_t D_801527A0;
ABS(D_8017A640, 0x8017A640);                    /* the end of the game's BSS: the heap starts there */
ABS(D_80000318, 0x80000318);                    /* osMemSize */
struct { uint8_t pad[72]; void **handle; } input_rec0[4];   /* the input records' profile handles */
int16_t active_player_count;
float D_8002AFB8 = 1.0f / 60;                   /* gRetraceTime */
volatile int8_t D_80114738;
int32_t D_80143FF4;
struct pc_model pc_models[PC_MODEL_SLOTS];    /* the model records (pc_game_types.h) */
#define D_8014A250 ((uint8_t (*)[0x808])pc_models)
struct pc_car pc_cars[PC_CAR_SLOTS];            /* the car records (pc_game_types.h) */
int16_t D_803B9BBA;
int8_t D_8011103C[13], D_80111080[13], D_8011119C[13], D_80111230[13], D_8011128C[13], D_80111604[13],
    D_80111648[13], D_8011168C[13], D_8011157C[13], D_801115C0[13];   /* the default car settings */
float D_801112DC[13], D_801113E0[13], D_80111754[32], D_8002AFB4 = 60;

/* ---------------------------------------------------------------- what the code calls: libultra and the rest */

void *n64_memcpy(void *d, const void *s, unsigned int n) { return memcpy(d, s, n); }
void *n64_memset(void *d, int c, unsigned int n) { return memset(d, c, n); }
unsigned int n64_strlen(const char *s) { return (unsigned int)strlen(s); }
void *func_800A47C0(void *d, const void *s, uint32_t n) { return memmove(d, s, n); }
void osCreateMesgQueue(void *q, void *m, int32_t n) {}
int32_t osSendMesg(void *q, void *m, int32_t flags) { return 0; }
int32_t osRecvMesg(void *q, void **m, int32_t flags) { return 0; }
void osCreateThread(void *t, int32_t id, void (*entry)(void *), void *arg, void *sp, int32_t pri) {}
void osStartThread(void *t) {}
void osInvalDCache(void *p, int32_t n) {}
void osWritebackDCache(void *p, int32_t n) {}
void audio_queue_process(void *arg) {}
void func_8008A704(void) {}
int32_t func_8008A6A4(int32_t pak) { return 0; }
int32_t osMotorInit(void *q, OSPfs *pfs, int32_t channel) { return 1; }
int32_t osPfsRepairId(OSPfs *pfs) { return 0; }
void PurgeableMemory(uint32_t p, int32_t n) {}
int8_t func_800CCB40(int32_t option) { return (int8_t)(option + 1); }   /* an option's default */
void func_800CCA04(void **player, int32_t n) {}
void func_800CC8C8(void **player, int32_t n) {}
int8_t reverb_setup(void **player, uint8_t option) { return 0; }
void update_viewport(int8_t x, int8_t y) {}
void func_800A510C(void) {}
void draw_text(void **player) {}
int32_t inflate_entry_alt(void *src, int32_t size, uint32_t dst) { return 0; }
void menu_item_select(void *h, int32_t n) {}
void func_800CB9D0(void *h) {}
void func_800CB748(void *header, void *data) {}
void func_80095EC0(void *p, uint32_t n) {}

/* The zip of CompressGhost: a copy, into a buffer of the test's. */
static uint8_t zbuf[4096];
void *sound_play_menu(void *heap, uint32_t size) { return zbuf; }
uint32_t car_angular_velocity_clamp(void *in, uint32_t n, void *out, uint32_t avail, int32_t level, int32_t bits,
                                    int32_t mem)
{
    memcpy(out, in, n);
    return n;
}

static int errors;
static void pak_error(int32_t pak, int32_t error, uint32_t blocks, int32_t a, int8_t *retry, int8_t *status, void *p)
{
    errors++;
    *retry = 0;
    *status = 0;
}

static intptr_t removed[8];      /* audio_channel_priority: a profile taken out of the high-score table */
static int nremoved;
void audio_channel_priority(intptr_t handle)
{
    if (nremoved < 8)
        removed[nremoved] = handle;
    nremoved++;
}

static uint32_t freed[32];       /* DeallocateMemory: the blocks and handles freed (the test's heap never frees) */
static int nfreed;
void audio_reverb_update(uint32_t address, int32_t tag)
{
    if (nfreed < 32)
        freed[nfreed] = address;
    nfreed++;
}
static int was_freed(uint32_t a)
{
    int i;
    for (i = 0; i < nfreed && i < 32; i++)
        if (freed[i] == a)
            return 1;
    return 0;
}

static uint32_t ghost_block_size;   /* GetPtrSize of the ghost's header handle */
void *audio_buffer_sync(uint32_t handle) { return (void *)(uintptr_t)ghost_block_size; }

/* ---------------------------------------------------------------- the pak: 16 notes */

static struct note {
    int used;
    uint16_t company;
    uint32_t game;
    uint8_t game_name[16], ext_name[4];
    uint32_t size;
    uint8_t data[4096];
} notes[16];
static int deletes;

static struct note *find(uint16_t company, uint32_t game, const uint8_t *name, const uint8_t *ext)
{
    int i;
    for (i = 0; i < 16; i++)
        if (notes[i].used && notes[i].company == company && notes[i].game == game &&
            !memcmp(notes[i].game_name, name, 16) && !memcmp(notes[i].ext_name, ext, 4))
            return &notes[i];
    return NULL;
}

int32_t osPfsInitPak(void *queue, OSPfs *pfs, int32_t channel)
{
    pfs->queue = queue;
    pfs->channel = channel;
    memset(pfs->id, 0x5A, sizeof pfs->id);
    return channel == PAK ? 0 : 1;
}

int32_t osPfsFreeBlocks(OSPfs *pfs, int32_t *bytes)
{
    int i, used = 0;
    for (i = 0; i < 16; i++)
        used += notes[i].used ? (int)((notes[i].size + 255) / 256) : 0;
    *bytes = (123 - used) * 256;
    return 0;
}

int32_t osPfsAllocateFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext, int32_t size,
                          int32_t *fileNo)
{
    int i;
    if (find(company, game, name, ext))
        return 9;   /* PFS_ERR_EXIST */
    for (i = 0; i < 16 && notes[i].used; i++)
        ;
    if (i == 16)
        return 8;   /* PFS_DIR_FULL */
    memset(&notes[i], 0, sizeof notes[i]);
    notes[i].used = 1, notes[i].company = company, notes[i].game = game, notes[i].size = (uint32_t)size;
    memcpy(notes[i].game_name, name, 16);
    memcpy(notes[i].ext_name, ext, 4);
    *fileNo = i;
    return 0;
}

struct pfs_state { uint32_t file_size, game_code; uint16_t company_code; uint8_t ext_name[4], game_name[16]; };
int32_t osPfsFileState(OSPfs *pfs, int32_t fileNo, struct pfs_state *st)
{
    if (!notes[fileNo].used)
        return 5;   /* PFS_ERR_INVALID */
    st->file_size = notes[fileNo].size;
    st->game_code = notes[fileNo].game;
    st->company_code = notes[fileNo].company;
    memcpy(st->ext_name, notes[fileNo].ext_name, 4);
    memcpy(st->game_name, notes[fileNo].game_name, 16);
    return 0;
}

int32_t osPfsReadWriteFile(OSPfs *pfs, int32_t fileNo, uint8_t write, int32_t offset, int32_t size, uint8_t *buf)
{
    if (!notes[fileNo].used || offset < 0 || offset + size > (int32_t)sizeof notes[fileNo].data)
        return 5;
    if (write)
        memcpy(notes[fileNo].data + offset, buf, (size_t)size);
    else
        memcpy(buf, notes[fileNo].data + offset, (size_t)size);
    return 0;
}

int32_t osPfsDeleteFile(OSPfs *pfs, uint16_t company, uint32_t game, uint8_t *name, uint8_t *ext)
{
    struct note *n = find(company, game, name, ext);
    if (!n)
        return 5;
    n->used = 0;
    deletes++;
    return 0;
}

/* ---------------------------------------------------------------- helpers */

static void run_server(int frames)
{
    while (frames--)
        SaveServer(0, 0);
}

/* The pak out of its port and back, as the pak thread's scan sees it. */
static void pak_out(void)
{
    D_80149440[PAK].status = 0;
    func_800A43FC();
    run_server(1);
}
static void pak_in(void)
{
    D_80149440[PAK].status = 1;   /* CONT_CARD_ON */
    func_800A43FC();
    run_server(1);
}

static int count(struct dllist *l)
{
    int n = 0;
    void **h;
    for (h = l->head; h && n < 100; h = *(void ***)*h)
        n++;
    return n;
}

static int free_handles(void) { return (int)D_801460E0.count; }

static void **find_file(const char *name)
{
    void **h;
    for (h = D_80144D60[PAK].head; h; h = FILE_OF(h)->next)
        if (!strcmp(FILE_OF(h)->name, name))
            return h;
    return NULL;
}

static struct note *note_named(const char *ascii, const char *ext)
{
    uint8_t name[16], e[4];
    func_800A150C(name, (uint8_t *)ascii, 16);
    func_800A150C(e, (uint8_t *)ext, 4);
    return find(0x3544, 0x4E525545, name, e);
}

/* ---------------------------------------------------------------- the tests */

static void **alice_file;

/* The heap's handles: a handle is its slot's address, and the game reads the slot as a pointer, so slots in a row
 * don't overlap. */
static void test_handles(void)
{
    uint8_t **a = AllocateHandleHeap(NULL, 64), **b = AllocateHandleHeap(NULL, 64);

    CHECK(IN_WINDOW(a) && IN_WINDOW(b) && a != b);
    CHECK(IN_WINDOW(*a) && IN_WINDOW(*b) && *a != *b && (*b - *a == 64 + 32 || *a - *b == 64 + 32));
}

/* PS_InitPakSys and the default profiles; PS_PakServer finds the pak, empty. */
static void test_init(void)
{
    car_lod_select(pak_error);
    CHECK(free_handles() == 64 && D_801460E0.indirect && D_801460E0.doubly);
    CHECK(FILE_OF(&D_80144C60[5]) == &D_80144DC0[5]);
    menu_save_options();
    CHECK(D_8012E6D8.doubly == 1 && D_80152020.doubly == 0 && D_8012E6D8.head == NULL);
    CHECK(D_80146150[2] == &D_801391F8[2] && D_801391F8[2].data == (uint8_t **)&D_801392F0[2]);
    CHECK(!strcmp(D_801391F8[2].name, "PLAYER 3"));            /* the default name, sprintf'd into the record */
    CHECK(!strcmp((char *)D_8013A028[2] + 13, "PLAYER 3"));      /* and into its save data */
    pak_in();
    run_server(3);
    CHECK(D_80144030[PAK].inserted == 1 && D_80144030[PAK].opaque4 == 1 && D_80144030[PAK].pfs.channel == PAK);
    CHECK(D_80144030[0].inserted == 0 && D_80144D60[PAK].head == NULL && errors == 0);
}

/* CreatePlayer: a profile record, its pak file record and the file's function table, and the save file on the
 * pak. */
static void test_create(void)
{
    void **player = CreatePlayer(PAK, (uint8_t *)"ALICE");
    struct profile *p;
    struct psfile *f;
    struct note *n;

    CHECK(player != NULL);
    if (!player)
        return;
    p = PROFILE_OF(player);
    CHECK(player == &D_8013C128[PAK][0] && p == &D_8013E5D8[PAK][0]);
    alice_file = p->fileh;
    CHECK(alice_file == D_80144D60[PAK].head && free_handles() == 63);
    f = FILE_OF(alice_file);
    CHECK(f->RemoveFile == (void (*)(void **))audio_volume_pan);
    CHECK(f->MoveFile == (void (*)(void **, void **))func_800949D4);
    CHECK(f->pak == PAK && f->fileNo == 0 && f->size == 2304 && f->blocks == 9);   /* whole pages */
    CHECK(!strcmp(f->name, "RUSH2049ALICE") && !strcmp(f->ext, "P"));
    CHECK(f->data != NULL && p->data == f->data && IN_WINDOW(f->data) && IN_WINDOW(*f->data));
    CHECK(!strcmp(p->name, "ALICE") && p->controls[10] == 1 && p->controls[0] == 1 && p->controls[9] == 7);
    CHECK(D_8012E6D8.head == player && D_8012E6D8.count == 1 && p->next == NULL && p->prev == NULL);
    n = note_named("RUSH2049ALICE", "P");
    CHECK(n && n->size == 2304);
    if (!n)
        return;
    /* the save file on the pak, in its N64 layout: written whole by CreatePlayer */
    CHECK(n->data[12] == 16 && !strcmp((char *)n->data + 13, "ALICE"));
    CHECK(U32(n->data, 0) == format_string_parse(n->data + 4, 24));
    CHECK(U32(n->data, 4) == (uint32_t)D_8002E8E8[159] && (U32(n->data, 8) & 7) == PAK);
    CHECK(!memcmp(p->id, n->data + 4, 8));
    CHECK(n->data[32] == 1 && n->data[32 + 20] == 21);                     /* options 0 and 20 */
    CHECK(U32(n->data, 28) == format_string_parse(n->data + 32, 21));
    CHECK(!memcmp(n->data, *f->data, 2064));                               /* nothing left unflushed */
}

/* The pak taken out: _RemovePak calls each file's RemoveFile, which takes the profile out of the list and the
 * high-score table; the file records go back on the free list. Put back, PS_PakServer finds the file and
 * SaveServer reads the profile in: _ReadPlayer, the check of every block, PurgePlayerData. */
static void test_reread(void)
{
    struct profile *p;
    void **player;

    nremoved = 0;
    pak_out();
    CHECK(D_8012E6D8.head == NULL && D_8012E6D8.count == 0);
    CHECK(nremoved == 1 && removed[0] == (intptr_t)&D_8013C128[PAK][0]);
    CHECK(free_handles() == 64 && D_80144D60[PAK].head == NULL && D_80144030[PAK].inserted == 0);
    memset(&D_8013E5D8[PAK][0], 0xEE, sizeof D_8013E5D8[PAK][0]);   /* nothing of the old record is reused */
    pak_in();
    player = D_8012E6D8.head;
    CHECK(player == &D_8013C128[PAK][0] && errors == 0);
    if (!player)
        return;
    p = PROFILE_OF(player);
    alice_file = find_file("RUSH2049ALICE");
    CHECK(alice_file && p->fileh == alice_file && !strcmp(p->name, "ALICE"));
    CHECK(p->controls[10] == 1 && p->controls[3] == 9);
    CHECK(p->data == NULL && FILE_OF(alice_file)->data == NULL);   /* read, checked and purged */
    CHECK(FILE_OF(alice_file)->RemoveFile == (void (*)(void **))audio_volume_pan);
    CHECK(FILE_OF(alice_file)->size == 2304 && FILE_OF(alice_file)->fileNo == 0);
    D_803B9BBA = PAK;                                            /* overlay A's file lookup */
    CHECK(func_80398BF0(0) == alice_file && func_80398BF0(3) == NULL);
    PS_ReadFile(alice_file);                                     /* the whole file, by its size */
    CHECK(FILE_OF(alice_file)->data && !strcmp((char *)*FILE_OF(alice_file)->data + 13, "ALICE"));
}

/* LoadPlayerData takes the profile's data from its file and saves its controls; a car setting and a byte written
 * through PS_WriteFile reach the pak through SaveFlush. MoveFile (func_800949D4, never called by the game) moves
 * the profile to another file handle. */
static void test_load_and_flush(void)
{
    void **player = D_8012E6D8.head;
    struct profile *p;
    struct psfile *f = FILE_OF(alice_file);
    void *other = &D_80144C60[40];
    struct note *n = note_named("RUSH2049ALICE", "P");
    uint8_t *data;

    CHECK(player != NULL && n != NULL && f->data != NULL);
    if (!player || !n || !f->data)
        return;
    p = PROFILE_OF(player);
    p->controls[2] = 99;
    CHECK(func_800CCEFC(player) == 1);
    CHECK(p->data == f->data);
    data = *f->data;
    CHECK(data[60 + 2] == 99 && U32(data, 56) == format_string_parse(data + 60, 11) && n->data[62] != 99);
    func_800C7578(player, 3, 2, 55);
    CHECK(data[1856 + 48 + 4 + 2] == 55);
    data[1700] = 0x77;
    PS_WriteFile(alice_file, data + 1700, 1);
    CHECK(D_80144030[PAK].files[0].flag1 == 1 && D_80144030[PAK].flush == 1);
    SaveFlush(0);
    CHECK(n->data[62] == 99 && U32(n->data, 56) == U32(data, 56));
    CHECK(n->data[1856 + 48 + 4 + 2] == 55 && U32(n->data, 1856 + 48) == format_string_parse(data + 1856 + 52, 12));
    CHECK(n->data[1700] == 0x77 && D_80144030[PAK].files[0].flag1 == 0 && D_80144030[PAK].flush == 0);

    f->MoveFile(alice_file, (void **)other);
    CHECK(p->fileh == other);
    f->MoveFile((void **)other, alice_file);
    CHECK(p->fileh == alice_file);
}

/* A ghost: CompressGhost makes it from the race's recording header (in a ghost slot that held another ghost),
 * SaveGhost writes it to the pak; out and back in, _ReadGhostHeader reads its header and _ReadGhostData its data;
 * the replay plays it and records through the header. */
static void test_ghost(void)
{
    static uint8_t rec[GH_BYTES];   /* the race's recording header (D_80142530's N64 layout) */
    const uint32_t max = 600;       /* (ghostRaceTimes[0] + 10) * 60 frames a part */
    void **ghosth, **old, **back, **fileh;
    struct ghost *g;
    uint8_t *h, *buf;
    int8_t blocks = 0;
    struct note *n;
    float t;
    int i;

    /* the recording: 4 frames of gear and flags, steering, and gas and brake, 600 apart */
    buf = AllocateMemoryHeap(NULL, 3 * max);
    CHECK(IN_WINDOW(buf));
    memset(buf, 0, 3 * max);
    buf[0] = 0x0B, buf[1] = 0x12, buf[2] = 0x0A, buf[3] = 0x01;
    buf[max] = 0x7F, buf[max + 1] = (uint8_t)-127, buf[max + 3] = 64;
    buf[2 * max] = 0xF0, buf[2 * max + 1] = 0x0F, buf[2 * max + 2] = 0x88;
    rec[GH_FLAG] = 0, rec[GH_TRACK] = 3, rec[GH_BACK] = 1;
    strcpy((char *)rec + GH_NAME, "ALICE");
    U32(rec, GH_ID) = 0x11112222, U32(rec, GH_ID + 4) = 0x33334444;
    F32(rec, GH_TIME) = 1.0f / 60, F32(rec, GH_FINISH) = 61.5f;
    U32(rec, GH_START) = W32(buf), U32(rec, GH_OFFSET) = 4, U32(rec, GH_COUNT) = 4;

    old = func_800C7200();                                       /* a ghost slot used before */
    memset(*old, 0xEE, PC_GHOST_SIZE);
    *old = NULL;
    ghost_block_size = GH_BYTES + 12;
    ghosth = CompressGhost(rec, &blocks);
    CHECK(ghosth == old && blocks == 1 && U32(rec, GH_START) == 0 && was_freed(W32(buf)));
    if (ghosth != old)
        return;
    g = GHOST_OF(ghosth);
    CHECK(g->track == 3 && g->backward == 1 && !strcmp(g->name, "ALICE") && g->finish == 61.5f);
    CHECK(g->playerId[0] == 0x11112222 && g->playerId[1] == 0x33334444);
    CHECK(g->ll == NULL && g->fileh == NULL && g->datah == NULL && g->header != NULL);
    h = *g->header;
    CHECK(IN_WINDOW(g->header) && IN_WINDOW(h) && h[GH_REV] == 17 && h[GH_TRACK] == 3);
    CHECK(U32(h, GH_START) == 0 && U32(h, GH_OFFSET) == 0 && U32(h, GH_COUNT) == 0);
    CHECK(U32(h, GH_ZSIZE) == 12 && U32(h, GH_SIZE) == 12 && U32(h, GH_ID) == 0x11112222);
    CHECK(U32(h, GH_SUM) == format_string_parse(h + GH_REV, GH_ZSUM - 4));
    CHECK(h[GH_BYTES] == 0x0B && h[GH_BYTES + 4] == 0x7F && h[GH_BYTES + 8] == 0xF0 && h[GH_BYTES + 11] == 0);

    CHECK(SaveGhost(PAK, D_8012E6D8.head, ghosth, 0) == 1);
    fileh = g->fileh;
    CHECK(fileh != NULL && D_80152020.head == ghosth && g->header == NULL && g->datah == NULL);
    if (!fileh)
        return;
    CHECK(FILE_OF(fileh)->RemoveFile == (void (*)(void **))audio_volume_pan);
    CHECK(FILE_OF(fileh)->MoveFile == (void (*)(void **, void **))func_800949D4);
    CHECK(!strcmp(FILE_OF(fileh)->name, "RUSH2049ALICE4B") && !strcmp(FILE_OF(fileh)->ext, "G"));
    CHECK(FILE_OF(fileh)->data == NULL);                         /* written and released */
    n = note_named("RUSH2049ALICE4B", "G");
    CHECK(n && n->size == 256);
    if (!n)
        return;
    CHECK(n->data[GH_REV] == 17 && !strcmp((char *)n->data + GH_NAME, "ALICE") && F32(n->data, GH_FINISH) == 61.5f);
    CHECK(U32(n->data, GH_ZSUM) == format_string_parse(n->data + GH_BYTES, 12) && n->data[GH_BYTES + 8] == 0xF0);
    U32(n->data, GH_OFFSET) = 0x12345678, U32(n->data, GH_COUNT) = 0x9ABCDEF0;   /* not part of the checksums */

    /* out and back in: the ghost goes with RemoveFile, and comes back from its header */
    pak_out();
    CHECK(D_80152020.head == NULL && *ghosth == NULL);           /* RemoveFile freed the ghost (_FreeGhost) */
    pak_in();
    back = D_80152020.head;
    CHECK(back != NULL && count(&D_80152020) == 1 && count(&D_8012E6D8) == 1);
    if (!back)
        return;
    g = GHOST_OF(back);
    fileh = find_file("RUSH2049ALICE4B");
    CHECK(g->fileh == fileh && g->track == 3 && g->backward == 1 && !strcmp(g->name, "ALICE"));
    CHECK(g->playerId[0] == 0x11112222 && g->playerId[1] == 0x33334444 && g->finish == 61.5f);
    CHECK(g->header == NULL && g->datah == NULL && FILE_OF(fileh)->data == NULL);

    /* _ReadGhostData: the whole file, checked (the data's checksum after the 88-byte header) */
    CHECK(func_800CBF2C(back, 0, 0) == 1);
    CHECK(g->header != NULL && g->header == FILE_OF(fileh)->data);
    if (!g->header)
        return;
    h = *g->header;
    CHECK(U32(h, GH_START) == 0 && U32(h, GH_OFFSET) == 0x12345678 && U32(h, GH_COUNT) == 0x9ABCDEF0);

    /* the replay: the header's start is the samples' window address, offset the frame, size the frames */
    U32(h, GH_START) = W32(h + GH_BYTES), U32(h, GH_OFFSET) = 0, U32(h, GH_COUNT) = 4;
    h[GH_FLAG] = 1;
    D_80152698[0] = back;
    CHECK(func_800E5D64(0, &t) == 0 && t == F32(h, GH_TIME));
    CHECK(U32(h, GH_OFFSET) == 1 && (int8_t)D_8014A250[0][0x730] == 2 && D_8014A250[0][0x732] == 1);
    CHECK(MODEL_F(0x720) == 1.0f && MODEL_F(0x72C) == 1.0f && MODEL_F(0x728) == 0.0f);   /* steer, gas, brake */
    for (i = 1; i < 4; i++)
        func_800E5D64(0, &t);
    CHECK(U32(h, GH_OFFSET) == 4);
    h[GH_FLAG] = (uint8_t)-1;                                    /* recording */
    U32(h, GH_OFFSET) = 2;
    D_8014A250[0][0x730] = 4, D_8014A250[0][0x732] = 1, D_8014A250[0][0x731] = 0;
    MODEL_F(0x720) = -1.0f, MODEL_F(0x72C) = 1.0f, MODEL_F(0x728) = 0.0f;
    func_800E5D64(0, &t);
    CHECK(h[GH_BYTES + 2] == (5 | 8) && (int8_t)h[GH_BYTES + 4 + 2] == -127 && h[GH_BYTES + 8 + 2] == 0xF0);
    D_80152698[0] = NULL;

    /* PS_DeleteFile through DeleteGhost: RemoveFile takes the ghost out */
    deletes = 0;
    DeleteGhost(back);
    CHECK(deletes == 1 && note_named("RUSH2049ALICE4B", "G") == NULL);
    CHECK(D_80152020.head == NULL && *back == NULL && find_file("RUSH2049ALICE4B") == NULL);
}

/* PS_DeleteFile of the profile's file: RemoveFile takes the profile out of the list and the high-score table. */
static void test_delete(void)
{
    void **player = D_8012E6D8.head;
    int before = free_handles();

    CHECK(player != NULL && alice_file != NULL);
    if (!player || !alice_file)
        return;
    nremoved = deletes = 0;
    input_rec0[0].handle = player;
    active_player_count = 1;
    pak_delete_file_retry(alice_file);
    CHECK(deletes == 1 && note_named("RUSH2049ALICE", "P") == NULL);
    CHECK(D_8012E6D8.head == NULL && nremoved == 1 && removed[0] == (intptr_t)player);
    CHECK(input_rec0[0].handle == NULL && free_handles() == before + 1 && D_80144D60[PAK].head == NULL);
    CHECK(D_80144030[PAK].files[0].file_size == 0 && errors == 0);
}

int main(void)
{
    uint8_t *w = test_rdram();
    int i;

    if (!w) {
        fprintf(stderr, "test_save: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    pc_fix_absolute_got();
    U32(w, 0x318) = 0x400000;                          /* osMemSize: 4 MB */
    gControllerData[PAK].present = 1;
    for (i = 0; i < 256; i++)                          /* the N64 pak font: 0, space, digits, letters */
        D_8011EAEC[i] = 0xFFFF;
    D_8011EAEC[0] = 0, D_8011EAEC[15] = ' ';
    for (i = 0; i < 10; i++)
        D_8011EAEC[16 + i] = (uint16_t)('0' + i);
    for (i = 0; i < 26; i++)
        D_8011EAEC[26 + i] = (uint16_t)('A' + i);
    D_8002E8E8[159] = 0x2049;
    func_800E7D0C(64, 64);
    test_handles();
    test_init();
    test_create();
    test_reread();
    test_load_and_flush();
    test_ghost();
    test_delete();
    return TEST_DONE();
}
