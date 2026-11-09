/* port/assets/tests/test_endian.c: byte order where the game writes a value at one width and reads it at another
 * (docs/port/ENDIAN.md), run through the game's own code on records built as the game builds them:
 * - scene-node handles: MBOX_NewObject's s32 result is kept whole and read back as its low s16 half through a
 *   union (the coin updater's Id and EntityHdr views, the car parts' Handle in SetupCarColor). On the N64 the
 *   low half is the second s16; here it must be the first;
 * - colours: the coin's gold and silver are R,G,B,A bytes in the ROM, which the PC loader swaps as the u32s they are
 *   declared; the scene node's colour, read as bytes by the model draw, must come out R,G,B,A again. A car's colour
 *   (SetFrameColor) goes to the six nodes its parts' handles name.
 * game: src/blob/AnimateCoin.c src/blob/groups/SetupCarColor/SetupCarColor.c src/blob/SetFrameColor.c
 * game: src/blob/ZOID_SetColor.c src/blob/ZOID_SetObjectDef.c
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
Node ObjInstanceList[600];

/* ---- AnimateCoin (the coin updater): its views. The visual is 0x28 bytes, its data the coin's target, a
 * level-object node (struct tTarget, the host layout) whose data is the coin's record (a heap block in the N64
 * layout): both pointers (docs/port/POINTERS.md 10.2). */
typedef struct {
    int32_t model;
    float matrix[3][3];
    float position[3];
    uint8_t pad52[8];
    float spin[3];
    uint16_t missing;
    int16_t number;
} Coin;

typedef struct {   /* the coin's visual (struct pc_visual): its data the coin's target */
    void *next;
    uint8_t pad8[8];
    uintptr_t entity;
    float timeStamp;
    void (*func)(void *, short);
} CoinNode;
void AnimateCoin(CoinNode *node, int16_t update);

int32_t gPauseState;
uint32_t coin_gold_glow_color = 0xFFC800FF, coin_silver_glow_color = 0xC8E6FFFF;   /* gold and silver as the PC loader leaves them */
int32_t gWhichGoldCoin, gWhichSilverCoin;
uint8_t gNumModels = 1;
int16_t gNumPlayers;                                           /* no players: the coin is released at once */
char coin_glow_object_name[] = "COIN_GLOWG1", gold_coin_object_name[] = "GOLDCOIN", silver_coin_object_name[] = "SILVERCOIN";
float unitmatrix[3][3];
struct pc_car pc_cars[PC_CAR_SLOTS];            /* the car records (pc_game_types.h) */
volatile float gFrameSeconds;
int32_t gTargetList;

static int16_t g_spawned[4];
static int g_nspawned;
void RemoveObjectTree(int16_t node, int32_t children, int32_t siblings)
{
    if (g_nspawned < 4)
        g_spawned[g_nspawned] = node;
    g_nspawned++;
}
void ReleaseEnviron(void *node, int32_t unlink) {}
void FreeStatLLE(void *pool, void *element) {}
int32_t MBOX_FindObject_WORK(char *name, int8_t first, int8_t last, int8_t complain) { return 0x123; }
int32_t ZOID_AddObject(int32_t id, void *mat, int16_t parent, int32_t flags) { return 9; }   /* MBOX_NewObject */
void CopyUV(void *src, void *dst) {}
uint32_t IsCoinFound(int32_t player, int32_t number) { return 1; }
void MBOX_HideObject(int32_t node, int32_t mode, uint32_t mask) {}
void RotateUV(void *spin, void *matrix) {}

/* ---- SetupCarColor.c (SetupCarColor, SetModelPalette): its views, host layout */
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
void SetupCarColor(int16_t index, int16_t palette, uint8_t first, uint8_t second, uint8_t third);
void SetFrameColor(int16_t index, uint32_t *color);   /* a car's colour on its six part nodes */

uint8_t gCarPaintJobInited[8];
uint32_t paint_colors[8];
Resource24 car_palettes[8];
Handles64 gCarParts[8];
char car_palette_name_format[] = "CARPAL%d";
int heap_lock_queue;
static uint16_t g_colors[256];
static Resource24 g_wad_palette;
void *AllocateMemoryHeap(void *heap, uint32_t size) { return g_colors; }
Resource24 *MBOX_FindPalette_WORK(char *name, uint16_t *handle, int8_t lo, int8_t hi, int32_t err)
{
    *handle = 0;
    return &g_wad_palette;
}
void *n64_memcpy(void *dst, const void *src, uint32_t n) { return memcpy(dst, src, n); }
void DeallocateMemoryInternal(uint32_t block, int32_t mode) {}
int osRecvMesg(void *mq, void **msg, int block) { return 0; }
int osSendMesg(void *mq, void *msg, int block) { return 0; }

int main(void)
{
    static CoinNode node;
    uint8_t *w = test_rdram();
    int i;

    if (w) {
        Coin *coin = (Coin *)(w + 0x310000);        /* a heap record, in the window */
        static struct tTarget target;               /* the coin's level-object node */
        struct tTarget *entity = &target;

        /* The coin: its scene node is 7, its model will be MBOX_NewObject's 9. */
        memset(ObjInstanceList, 0, sizeof(ObjInstanceList));
        memset(coin, 0, sizeof *coin);
        memset(entity, 0, sizeof *entity);
        entity->Flags = 0x40;   /* gold, first update */
        entity->dlIdx = 7;      /* its glow: a scene node handle */
        entity->data = coin;
        node.entity = (uintptr_t)entity;
        AnimateCoin(&node, 1);
        CHECK(ObjInstanceList[7].model == 0x123);                  /* resource_set_object(id.part.index) */
        CHECK(memcmp(ObjInstanceList[7].prim, "\xFF\xC8\x00\xFF", 4) == 0);   /* gold: R,G,B,A for the model draw */
        CHECK(coin->model == 9);
        CHECK(g_nspawned == 2 && g_spawned[0] == 9 && g_spawned[1] == 7);   /* EntityHdr and id.part.index */
        CHECK(ObjInstanceList[0].model == 0 && memcmp(ObjInstanceList[0].prim, "\0\0\0\0", 4) == 0);

        /* Silver, a second coin. */
        entity->Flags = 0;
        entity->dlIdx = 12;
        g_nspawned = 0;
        AnimateCoin(&node, 1);
        CHECK(memcmp(ObjInstanceList[12].prim, "\xC8\xE6\xFF\xFF", 4) == 0);
        CHECK(g_nspawned == 2 && g_spawned[0] == 9 && g_spawned[1] == 12);
    } else {
        fprintf(stderr, "no RDRAM window: the coin part is skipped\n");
    }

    /* A car's colour, a word copied whole: each part's node gets the bytes as they are. */
    memset(ObjInstanceList, 0, sizeof(ObjInstanceList));
    memset(gCarParts, 0, sizeof(gCarParts));
    gCarParts[2].root = 21;
    gCarParts[2].secondary = 22;
    for (i = 0; i < 4; i++)
        gCarParts[2].unknown18[1 + i] = 23 + i;   /* the parts at +0x1C..+0x28 */
    {
        uint8_t rgba[4] = {0x12, 0x34, 0x56, 0x78};
        uint32_t color;
        memcpy(&color, rgba, 4);
        SetFrameColor(2, &color);
        for (i = 21; i <= 26; i++)
            CHECK(memcmp(ObjInstanceList[i].prim, rgba, 4) == 0);
        CHECK(memcmp(ObjInstanceList[0].prim, "\0\0\0\0", 4) == 0);
    }

    /* A car's paint job: the palette descriptor goes to the car's two model nodes, not to node 0. */
    if (!w) {
        fprintf(stderr, "no RDRAM window: the car palette part is skipped\n");
        return TEST_DONE();
    }
    for (i = 0; i < 256; i++)
        test_put16(w + 0x200000 + 2 * i, (uint16_t)(0x8000 | i));   /* the WAD's TLUT, big-endian */
    g_wad_palette.data = PC_RDRAM_BASE + 0x200000u;     /* window addresses */
    car_palettes[3].data = PC_RDRAM_BASE + 0x201000u;   /* the car's descriptor block */
    gCarParts[3].root = 42;
    gCarParts[3].secondary = 43;
    memset(ObjInstanceList, 0, sizeof(ObjInstanceList));
    SetupCarColor(3, 0, 1, 1, 1);
    CHECK(ObjInstanceList[42].pal == &car_palettes[3] && ObjInstanceList[43].pal == &car_palettes[3]);
    CHECK(ObjInstanceList[0].pal == NULL);
    CHECK(((uint16_t *)(w + 0x201000))[5] == (0x8000 | 5));   /* the TLUT copied in native order */
    return TEST_DONE();
}
