/* port/data/test_overlay_data.c: a runtime image's data put back on each load of the image (docs/port/DATA.md 5),
 * through the real pieces: the loader (pc_data.c) with tables of its own whose C ranges are port/data/overlays.ld's,
 * linked with that script, and two objects it takes as runtime-image C by their paths:
 *
 *   - image A: test_overlay/src/ovl_a/statics.c, a stand-in with file-local and function-local statics;
 *   - image B: the game's weapons.c, built as the game is. Its global table weaponOffsets is initialized data in its C
 *     range. Its muzzle-flash records and current weapons (gCurWeaponType, gGattMuzzleObjs, gCannMuzzleObjs) are image B's BSS,
 *     which this test's tables give an arena (in host memory) as the game's give the window: they start out zero,
 *     so the first _RemoveCannMFlash / _RemoveGattMFlash of a slot removes its models and marks them -1, and a
 *     second one finds nothing.
 *
 * The game writes them; a load of the other image leaves them alone; a load of their image puts them back, as the
 * N64's load does. The program stands in for everything else weapons.c references. */
#include "pc_data.h"
#include "pc_n64_map.h"   /* N64_OVERLAY_BASE: where the runtime images load */
#include "../include/pc_game_types.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* ---------------------------------------------------------------- what weapons.c references */

volatile float gFrameSeconds = 1.0f / 30;
__attribute__((aligned(16))) uint8_t gCarParts[6 * 0x40];      /* CarParts[] */
uint16_t gObjList[224];                                         /* the object definitions: the weapons' at 216..223 */
struct pc_model pc_models[PC_MODEL_SLOTS];     /* model[] (pc_game_types.h) */
__attribute__((aligned(16))) uint8_t gTexDefPtrs[64 * 16];       /* TexDefSet[] */
struct pc_car pc_cars[PC_CAR_SLOTS];                           /* the car records (pc_game_types.h) */
uint16_t gRipTextures[5];
int32_t gWfxObjList[7];

/* Image B's BSS arena for the weapons' state, from gCurWeaponType (0xED18 bytes into the image) to DamagePolys
 * (0x958 bytes on), the game's layout: here in host memory. */
#define ARENA_B_LO (N64_OVERLAY_BASE + 0xED18u)
#define ARENA_B_HI (ARENA_B_LO + 0x958u)
__attribute__((aligned(16))) uint8_t pc_test_arena_b[ARENA_B_HI - ARENA_B_LO];
#define ALIAS(name, off) __asm__(".globl " #name "\n.set " #name ", pc_test_arena_b + " #off)
ALIAS(gCurWeaponType, 0);
ALIAS(gGattMuzzleObjs, 0x8);
ALIAS(gCannMuzzleObjs, 0x438);

static int g_removed;
void ZOID_RemoveObject(int16_t obj) { g_removed++; }
float Random(float scale) { return 0.0f; }
void ScaleUV(float *src, float *dst, float s) {}
void *ZOID_FindMatrix(int16_t idx) { static float m[12]; return m; }
void ZOID_UpdateObject(int16_t obj, float *pos, int32_t a) {}
void ZOID_SetTexture(int16_t obj, void *tex, int32_t a) {}
void ZOID_SetColor(int16_t obj, void *clr) {}
void ZOID_SetObjectDef(int16_t obj, int32_t def) {}
void RollUV(float a, float *uvs) {}
void CopyUV(float *src, float *dst) {}
void MBOX_HideObject(int32_t obj, int32_t a, uint32_t mask) {}
void MBOX_ShowObject(int32_t obj, int32_t a, uint32_t mask) {}
int32_t MBOX_NewObject(int32_t obj, void *mat, int32_t parent, uint32_t flags) { return 1; }

void RemoveCannMFlash(int16_t slot);   /* _RemoveCannMFlash */
void RemoveGattMFlash(int16_t slot);   /* _RemoveGattMFlash */
extern float weaponOffsets[8][13][3];
int ovl_test_step(void);
int ovl_test_counter(void);
float ovl_test_speeds(void);

/* ---------------------------------------------------------------- tables: the two runtime images, raw */

extern uint8_t pc_ovl_a_start[], pc_ovl_a_end[], pc_ovl_b_start[], pc_ovl_b_end[];
#define IMAGE_SIZE (ARENA_B_LO - N64_OVERLAY_BASE)   /* image B's data ends where its BSS arena starts */
static uint8_t rom[2 * IMAGE_SIZE];
static const PcDataImage images[2] = {
    {"ovl_a", PC_IMG_RAW, 0, IMAGE_SIZE, IMAGE_SIZE, N64_OVERLAY_BASE},
    {"ovl_b", PC_IMG_RAW, IMAGE_SIZE, IMAGE_SIZE, IMAGE_SIZE, N64_OVERLAY_BASE},
};
/* The game heap's start in these tables: below the runtime images, which the game loads into its heap (the game's is
 * the end of its BSS, game_bss_end). */
#define TEST_HEAP (N64_KSEG0 + 0x17A640u)
static PcDataArena arenas[1] = {{1, ARENA_B_LO, ARENA_B_LO, ARENA_B_HI, pc_test_arena_b, 0, 0}};
static const PcDataCRange cranges[2] = {{0, pc_ovl_a_start, pc_ovl_a_end}, {1, pc_ovl_b_start, pc_ovl_b_end}};
static PcDataState state;
static const PcDataTables tables = {images, 2, arenas, 1, NULL, 0, NULL, 0, NULL, 0, NULL, 0, TEST_HEAP, &state,
                                    cranges, 2};

static int in(const void *p, const uint8_t *lo, const uint8_t *hi)
{
    return (const uint8_t *)p >= lo && (const uint8_t *)p < hi;
}

static int removed_cann(int16_t slot)
{
    g_removed = 0;
    RemoveCannMFlash(slot);
    return g_removed;
}

static int removed_gatt(int16_t slot)
{
    g_removed = 0;
    RemoveGattMFlash(slot);
    return g_removed;
}

int main(void)
{
    /* the C ranges: weapons.c's data in image B's, the stand-in's statics in image A's, the program's in neither */
    CHECK(in(weaponOffsets, pc_ovl_b_start, pc_ovl_b_end) && in(&weaponOffsets[7][12][2], pc_ovl_b_start, pc_ovl_b_end));
    CHECK(pc_ovl_a_end > pc_ovl_a_start && !in(weaponOffsets, pc_ovl_a_start, pc_ovl_a_end));
    CHECK(!in(&g_removed, pc_ovl_a_start, pc_ovl_b_end) && !in(pc_test_arena_b, pc_ovl_a_start, pc_ovl_b_end));

    CHECK(pc_data_load_all(&tables, rom, sizeof rom, "") == 0);
    CHECK(weaponOffsets[0][0][1] == 2.5f && weaponOffsets[7][12][2] == -1.11f);
    CHECK(ovl_test_counter() == 5 && ovl_test_speeds() == 0.0f);

    /* the game writes: image B's records (the arena) and table (the C range), image A's statics */
    CHECK(removed_cann(2) == 6 && removed_cann(2) == 0);   /* five muzzle models and the rip */
    CHECK(removed_gatt(3) == 5 && removed_gatt(3) == 0);
    weaponOffsets[0][0][1] = 99.0f;
    CHECK(ovl_test_step() == 1 && ovl_test_step() == 3);
    CHECK(ovl_test_counter() == 6 && ovl_test_speeds() == 3.0f);

    /* a load of image A puts back its statics, file-local and function-local, and leaves image B alone */
    CHECK(pc_data_load_image(&tables, 0) == 0);
    CHECK(ovl_test_counter() == 5 && ovl_test_speeds() == 0.0f);
    CHECK(ovl_test_step() == 1 && ovl_test_counter() == 6);
    CHECK(removed_cann(2) == 0 && removed_gatt(3) == 0 && weaponOffsets[0][0][1] == 99.0f);

    /* a load of image B puts back its data and zeroes its BSS, and leaves image A alone */
    CHECK(pc_data_load_image(&tables, 1) == 0);
    CHECK(weaponOffsets[0][0][1] == 2.5f && weaponOffsets[7][12][2] == -1.11f);
    CHECK(removed_cann(2) == 6 && removed_gatt(3) == 5 && removed_cann(2) == 0);
    CHECK(ovl_test_step() == 3 && ovl_test_counter() == 6);

    /* and again, after the game wrote them once more */
    CHECK(pc_data_load_image(&tables, 1) == 0 && pc_data_load_image(&tables, 0) == 0);
    CHECK(removed_cann(2) == 6 && removed_cann(1) == 6 && ovl_test_step() == 1);

    if (failures)
        fprintf(stderr, "test_overlay_data: %d failed\n", failures);
    return failures ? 1 : 0;
}
