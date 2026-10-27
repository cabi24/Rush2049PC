/* port/assets/tests/test_battle.c: the code that walks the level-object nodes (the targets) outside level_objects,
 * through the game's own code (docs/port/POINTERS.md 10). The nodes keep their N64 layout in the arena block
 * D_8014D280 and link each other by 32-bit window addresses (the node pool's n64 flag), so a walk of the active list
 * D_80143FD8 and a view of a node must read them that way: battle mode's power-up selection func_8039133C, which
 * keeps the flagged nodes in a heap block of slots, UpdateBattleHealth, UpdateBattleShield and UpdateBattleInvs
 * (runtime_b_resource_callbacks: a node's flags, handle and resource), and func_800BEA6C, which gives every node on
 * the list state 2 and no sound. The nodes and the heap sit in an RDRAM window the test maps (exit 77, skipped, if
 * it can't).
 * game: src/blob/camera_lerp_position.c src/blob/pool_linked_list_init.c src/blob/func_8008E3C0.c
 * game: src/blob/func_800BEA6C.c src/ovl_b/func_8039133C.c src/ovl_b/groups/runtime_b_resource_callbacks/group.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* A StatLL header on the PC (shots.c's StatLL) with the n64 flag. */
typedef struct { uint8_t doubly, n64; int32_t count, size; void *mem, *active, *free; } StatLL;
/* func_8039133C's slot: the node, then its state. */
typedef struct { void *object; int16_t active, count; } Slot;

#define NF(n, off, T) (*(T *)((uint8_t *)(n) + (off)))   /* a node field at its N64 offset */
#define W32(p) ((uint32_t)(uintptr_t)(p))
#define WP(a) ((void *)(uintptr_t)(a))
#define ABS(name, addr) __asm__(".globl " #name "\n.set " #name ", " #addr)
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data */

#define NODES 0x8014D280u            /* D_8014D280, 130 nodes of 0x70 bytes in the game arena */
#define NODE(i) ((uint8_t *)WP(NODES + 0x70u * (i)))
ABS(D_8014D280, 0x8014D280);
#define HEAP 0x80300000u             /* the game heap (AllocateMemoryHeap, audio_dma_sync) */

StatLL D_80143FC8, D_80151AA8;
ALIAS(D_80143FD8, D_80143FC8, 0x18);  /* the pool's active list, N64 +0x10 */
int8_t D_80114650, D_80150DD0;
int32_t D_80150B64, D_80151408, D_8014A110, D_80150F78, D_80151688, D_801515F0, D_80151964, D_80151610;
uint8_t D_80150E28[8];
intptr_t D_80151528[4][5];
void *D_80151A70;
/* gBattlePowerUps, the State of func_8039133C and runtime_b_resource_callbacks. */
struct {
    int8_t count, selected_delay, selected_b, selected_a;
    float delay, phase_a, phase_b;
    Slot *slots;
} D_80399AE0;
volatile float D_8002EB94 = 0.5f;    /* gFrameSeconds */
uint16_t D_80142A82 = 0x82, D_80142A84 = 0x84, D_80142A8E = 0x8E;
int16_t D_8014A108;                  /* gNumPlayers: 0, so UpdateInvs leaves the cars alone */
uint8_t D_80152818[6 * PC_CAR_SIZE] __attribute__((aligned(16)));

/* ---------------------------------------------------------------- what the code under test calls */

static uint32_t g_heap = HEAP;
void *AllocateMemoryHeap(int32_t heap, int32_t size)
{
    void *p = WP(g_heap);
    memset(p, 0, (size_t)size);
    g_heap += (uint32_t)size;
    memset(WP(g_heap), 0xAB, 64);    /* a guard after each block */
    return p;
}
static uint32_t g_slots_size;
void *audio_dma_sync(void *heap, uint32_t size)
{
    g_slots_size = size;
    return AllocateMemoryHeap(0, (int32_t)size);
}
float func_8008B2E4(float max) { return 0.0f; }   /* Random */
static int16_t g_def_index;
static uint16_t g_def;
void func_80090770(int16_t index, uint16_t def) { g_def_index = index, g_def = def; }   /* ZOID_SetObjectDef */
static int32_t g_shown;
void model_transform_setup(int32_t handle, int32_t a, uint32_t b) { g_shown = handle; }   /* MBOX_ShowObject */
void func_800BB7F4(void) {}
void func_803914A8(void) {}

/* ---------------------------------------------------------------- the game's functions */

void camera_lerp_position(int32_t arg0);
void *func_8008E3C0(void *pool);
void func_8039133C(int32_t reuse);
void func_80390D38(void);            /* UpdateBattleHealth */
void func_80390B10(void);            /* UpdateBattleShield */
void func_80390B08(void);            /* UpdateBattleInvs */
void func_800BEA6C(void);

static int guard_intact(const void *end)
{
    const uint8_t *g = end;
    for (int i = 0; i < 64; i++)
        if (g[i] != 0xAB)
            return 0;
    return 1;
}

int main(void)
{
    uint8_t *n[5];
    Slot *s;
    int i;

    if (!test_rdram()) {
        fprintf(stderr, "test_battle: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    pc_fix_absolute_got();
    memset(NODE(0), 0, 130 * 0x70);
    D_8014A110 = 0;
    camera_lerp_position(0);
    CHECK(D_80143FC8.n64 == 1 && D_80143FC8.free == NODE(0));

    /* Five nodes on the active list (GetStatLLE puts each at its head: 4, 3, 2, 1, 0). Nodes 4, 3 and 1 are
     * power-ups (flag 0x20); every handle has an upper half, so only its low half is the scene node. */
    for (i = 0; i < 5; i++) {
        n[i] = func_8008E3C0(&D_80143FC8);
        CHECK(n[i] == NODE(i));
        NF(n[i], 0x08, int32_t) = 0x4100 + i;                 /* key */
        NF(n[i], 0x0C, int32_t) = 0x00050100 + i;             /* scene-node handle */
        NF(n[i], 0x60, int32_t) = 7;                          /* sound */
    }
    CHECK(D_80143FC8.active == n[4] && NF(n[4], 0, uint32_t) == W32(n[3]) && NF(n[0], 0, uint32_t) == 0);
    NF(n[4], 4, uint8_t) = 0x20, NF(n[3], 4, uint8_t) = 0x28, NF(n[2], 4, uint8_t) = 0x01;
    NF(n[1], 4, uint8_t) = 0x20, NF(n[0], 4, uint8_t) = 0x10;
    memset(NODE(5), 0, 0x70);                                  /* a free node */

    /* The selection: one slot for each power-up, in list order, in a block of the slots' size. */
    D_80399AE0.count = 3;
    func_8039133C(0);
    s = D_80399AE0.slots;
    CHECK(g_slots_size >= 3 * sizeof(Slot));
    CHECK(guard_intact((uint8_t *)s + g_slots_size));
    CHECK(s[0].object == n[4] && s[1].object == n[3] && s[2].object == n[1]);
    for (i = 0; i < 3; i++)
        CHECK(s[i].active == 1 && s[i].count == 1);
    CHECK(D_80399AE0.delay == 5.0f && D_80399AE0.phase_a == 60.0f && D_80399AE0.phase_b == 15.0f);

    /* UpdateBattleHealth picks slot 0 (Random gives 0): node 4 is taken, gets resource 353 and the health
     * object definition on its scene node. */
    D_80399AE0.delay = 0.25f;
    func_80390D38();
    CHECK(D_80399AE0.selected_delay == 0 && s[0].active == 1 && s[0].count == 4 && D_80399AE0.delay == -2.0f);
    CHECK(NF(n[4], 4, uint8_t) == 0x22 && NF(n[4], 0x50, int16_t) == 353);
    CHECK(g_def_index == 0x104 && g_def == 0x82 && g_shown == 0x00050104);
    CHECK(NF(n[4], 0x08, int32_t) == 0x4104 && NF(n[4], 0x0C, int32_t) == 0x00050104);

    /* UpdateBattleShield goes past the taken slot to slot 1, node 3. */
    D_80399AE0.phase_b = 0.25f;
    func_80390B10();
    CHECK(D_80399AE0.selected_b == 1 && s[1].count == 3 && D_80399AE0.phase_b == -2.0f);
    CHECK(NF(n[3], 4, uint8_t) == 0x2A && NF(n[3], 0x50, int16_t) == 359);
    CHECK(g_def_index == 0x103 && g_def == 0x8E && g_shown == 0x00050103);

    /* UpdateBattleInvs takes the last one, node 1. */
    D_80399AE0.phase_a = 0.25f;
    func_80390B08();
    CHECK(D_80399AE0.selected_a == 2 && s[2].count == 2 && D_80399AE0.phase_a == -2.0f);
    CHECK(NF(n[1], 4, uint8_t) == 0x22 && NF(n[1], 0x50, int16_t) == 354);
    CHECK(g_def_index == 0x101 && g_def == 0x84 && g_shown == 0x00050101);
    CHECK(NF(n[2], 4, uint8_t) == 0x01 && NF(n[0], 4, uint8_t) == 0x10);   /* not power-ups: untouched */

    /* func_800BEA6C: every node on the list, and only those, gets state 2 and sound -1. */
    func_800BEA6C();
    for (i = 0; i < 5; i++)
        CHECK(NF(n[i], 0x64, int8_t) == 2 && NF(n[i], 0x60, int32_t) == -1);
    CHECK(NF(NODE(5), 0x64, int8_t) == 0 && NF(NODE(5), 0x60, int32_t) == 0);
    CHECK(NF(n[4], 0, uint32_t) == W32(n[3]) && NF(n[4], 4, uint8_t) == 0x22);
    return TEST_DONE();
}
