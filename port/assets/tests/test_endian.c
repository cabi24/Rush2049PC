/* port/assets/tests/test_endian.c: byte order where the game writes a value at one width and reads it at another
 * (docs/port/ENDIAN.md), run through the game's own code on records built as the game builds them:
 * - scene-node handles: MBOX_NewObject's s32 result is kept whole and read back as its low s16 half through a
 *   union (the coin updater's Id and EntityHdr views, the car parts' Handle in sound_bank_unload). On the N64 the
 *   low half is the second s16; here it must be the first;
 * - colours: the coin's gold and silver are R,G,B,A bytes in the ROM, which the PC loader swaps as the u32s they are
 *   declared; the scene node's colour, read as bytes by the model draw, must come out R,G,B,A again. A car's colour
 *   (func_80092FE0) goes to the six nodes its parts' handles name.
 * game: src/blob/func_8010E0FC.c src/blob/groups/carcolor/carcolor.c src/blob/func_80092FE0.c
 */
#include <string.h>

#include "pc_game_types.h"
#include "support.h"

/* particle_knot's Node: the layout every PC view of the scene-node table has (docs/port/POINTERS.md 9.2). */
typedef struct {
    uint32_t flags, lights;
    void *obj;
    float scale, w;
    uint16_t model;
    int16_t child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    uint8_t prim[4], env[4];
} Node;
_Static_assert(sizeof(Node) == 0x68, "the node's host size");
Node D_8012E700[600];

/* ---- func_8010E0FC (the coin updater): its views. The visual is 0x20 bytes, its data the coin's target, a
 * level-object node in its N64 layout whose state (+0x6C) is the coin record: both window addresses
 * (docs/port/POINTERS.md 10). */
typedef struct {
    int32_t model;
    float matrix[3][3];
    float position[3];
    uint8_t pad52[8];
    float spin[3];
    uint16_t missing;
    int16_t number;
} Coin;
typedef struct {
    uint8_t pad0[4];
    uint8_t flags;
    uint8_t pad5[7];
    int32_t handle;   /* Id: MBOX_NewObject's s32 */
    uint8_t pad16[40];
    float position[3];
    uint8_t pad68[40];
    uint32_t coin;
} Entity;
typedef struct {
    void *next;
    uint8_t pad8[8];
    uint32_t entity;
    float timeStamp;
    void (*func)(void *, short);
} CoinNode;
void func_8010E0FC(CoinNode *node, int16_t update);

int32_t D_801170FC;
uint32_t D_80118E14 = 0xFFC800FF, D_80118E18 = 0xC8E6FFFF;   /* gold and silver as the PC loader leaves them */
int32_t D_80151610, D_80151964;
uint8_t gNumModels = 1;
int16_t D_8014A108;                                           /* no players: the coin is released at once */
char D_80121DB0[] = "COIN_GLOWG1", D_80121DBC[] = "GOLDCOIN", D_80121DCC[] = "SILVERCOIN";
float D_8011418C[3][3];
struct pc_car pc_cars[PC_CAR_SLOTS];            /* the car records (pc_game_types.h) */
volatile float D_8002EB94;
int32_t D_80143FC8;

static int16_t g_spawned[4];
static int g_nspawned;
void entity_spawn_callback(int16_t node, int32_t children, int32_t siblings)
{
    if (g_nspawned < 4)
        g_spawned[g_nspawned] = node;
    g_nspawned++;
}
void entity_transform_apply(void *node, int32_t unlink) {}
void func_800AFA84(void *pool, void *element) {}
int32_t string_copy_format(char *name, int8_t first, int8_t last, int8_t complain) { return 0x123; }
int32_t func_8008E26C(int32_t id, void *mat, int16_t parent, int32_t flags) { return 9; }   /* MBOX_NewObject */
void math_utility(void *src, void *dst) {}
uint32_t func_800F7A98(int32_t player, int32_t number) { return 1; }
void model_data_load(int32_t node, int32_t mode, uint32_t mask) {}
void sound_position_set(void *spin, void *matrix) {}

/* ---- carcolor.c (sound_bank_unload, SetModelPalette): its views, host layout */
typedef struct {
    uint8_t directory_fields[20];
    uint32_t data;   /* a window address */
} Resource24;
typedef struct {
    int32_t root;
    uint32_t unknown04[3];
    int32_t secondary, body;
    uint32_t unknown18[10];
} Handles64;
void sound_bank_unload(int16_t index, int16_t palette, uint8_t first, uint8_t second, uint8_t third);
void func_80092FE0(int16_t index, uint32_t *color);   /* a car's colour on its six part nodes */

uint8_t D_8013FE90[8];
uint32_t D_8013F300[8];
Resource24 D_8013FEE0[8];
Handles64 D_80139320[8];
volatile uint8_t D_80140BDC = 1;
char D_80123498[] = "CARPAL%d";
int D_80152770;
static uint16_t g_colors[256];
static Resource24 g_wad_palette;
void *AllocateMemoryHeap(void *heap, uint32_t size) { return g_colors; }
Resource24 *MBOX_FindPalette_WORK(char *name, uint16_t *handle, int8_t lo, int8_t hi, int32_t err)
{
    *handle = 0;
    return &g_wad_palette;
}
void *n64_memcpy(void *dst, const void *src, uint32_t n) { return memcpy(dst, src, n); }
void audio_reverb_update(uint32_t block, int32_t mode) {}
int osRecvMesg(void *mq, void **msg, int block) { return 0; }
int osSendMesg(void *mq, void *msg, int block) { return 0; }

int main(void)
{
    static CoinNode node;
    uint8_t *w = test_rdram();
    int i;

    if (w) {
        Coin *coin = (Coin *)(w + 0x310000);        /* a heap record and a level-object node, in the window */
        Entity *entity = (Entity *)(w + 0x310100);

        /* The coin: its scene node is 7, its model will be MBOX_NewObject's 9. */
        memset(D_8012E700, 0, sizeof(D_8012E700));
        memset(coin, 0, sizeof *coin);
        memset(entity, 0, sizeof *entity);
        entity->flags = 0x40;   /* gold, first update */
        entity->handle = 7;
        entity->coin = (uint32_t)(uintptr_t)coin;
        node.entity = (uint32_t)(uintptr_t)entity;
        func_8010E0FC(&node, 1);
        CHECK(D_8012E700[7].model == 0x123);                  /* resource_set_object(id.part.index) */
        CHECK(memcmp(D_8012E700[7].prim, "\xFF\xC8\x00\xFF", 4) == 0);   /* gold: R,G,B,A for the model draw */
        CHECK(coin->model == 9);
        CHECK(g_nspawned == 2 && g_spawned[0] == 9 && g_spawned[1] == 7);   /* EntityHdr and id.part.index */
        CHECK(D_8012E700[0].model == 0 && memcmp(D_8012E700[0].prim, "\0\0\0\0", 4) == 0);

        /* Silver, a second coin. */
        entity->flags = 0;
        entity->handle = 12;
        g_nspawned = 0;
        func_8010E0FC(&node, 1);
        CHECK(memcmp(D_8012E700[12].prim, "\xC8\xE6\xFF\xFF", 4) == 0);
        CHECK(g_nspawned == 2 && g_spawned[0] == 9 && g_spawned[1] == 12);
    } else {
        fprintf(stderr, "no RDRAM window: the coin part is skipped\n");
    }

    /* A car's colour, a word copied whole: each part's node gets the bytes as they are. */
    memset(D_8012E700, 0, sizeof(D_8012E700));
    memset(D_80139320, 0, sizeof(D_80139320));
    D_80139320[2].root = 21;
    D_80139320[2].secondary = 22;
    for (i = 0; i < 4; i++)
        D_80139320[2].unknown18[1 + i] = 23 + i;   /* the parts at +0x1C..+0x28 */
    {
        uint8_t rgba[4] = {0x12, 0x34, 0x56, 0x78};
        uint32_t color;
        memcpy(&color, rgba, 4);
        func_80092FE0(2, &color);
        for (i = 21; i <= 26; i++)
            CHECK(memcmp(D_8012E700[i].prim, rgba, 4) == 0);
        CHECK(memcmp(D_8012E700[0].prim, "\0\0\0\0", 4) == 0);
    }

    /* A car's paint job: the palette descriptor goes to the car's two model nodes, not to node 0. */
    if (!w) {
        fprintf(stderr, "no RDRAM window: the car palette part is skipped\n");
        return TEST_DONE();
    }
    for (i = 0; i < 256; i++)
        test_put16(w + 0x200000 + 2 * i, (uint16_t)(0x8000 | i));   /* the WAD's TLUT, big-endian */
    g_wad_palette.data = 0x80200000u;
    D_8013FEE0[3].data = 0x80201000u;   /* the car's descriptor block */
    D_80139320[3].root = 42;
    D_80139320[3].secondary = 43;
    memset(D_8012E700, 0, sizeof(D_8012E700));
    sound_bank_unload(3, 0, 1, 1, 1);
    CHECK(D_8012E700[42].pal == &D_8013FEE0[3] && D_8012E700[43].pal == &D_8013FEE0[3]);
    CHECK(D_8012E700[0].pal == NULL);
    CHECK(((uint16_t *)(w + 0x201000))[5] == (0x8000 | 5));   /* the TLUT copied in native order */
    return TEST_DONE();
}
