/* port/assets/tests/test_battle.c: the code that walks the level-object nodes (the targets) outside level_objects,
 * through the game's own code (docs/port/POINTERS.md 10). The nodes keep their N64 layout in the arena block
 * gTargetArray and link each other by 32-bit window addresses (the node pool's n64 flag), so a walk of the active list
 * (gTargetList's head) and a view of a node must read them that way: battle mode's power-up selection InitBattlePowerUps, which
 * keeps the flagged nodes in a heap block of slots, UpdateBattleHealth, UpdateBattleShield and UpdateBattleInvs
 * (runtime_b_resource_callbacks: a node's flags, handle and resource), PauseTargets, which gives every node on the
 * list state 2 and no sound, and the pause screen's PauseTargets, which does the same when a race is paused
 * (updatePauseScreen, the nonmatching draft the PC builds). The nodes and the heap sit in an RDRAM window the
 * test maps (exit 77, skipped, if it can't).
 * game: src/blob/initTargetStructs.c src/blob/ClearStatLL.c src/blob/GetStatLLE.c
 * game: src/blob/PauseTargets.c src/ovl_b/InitBattlePowerUps.c src/ovl_b/groups/powerups/powerups.c
 * game: nonmatching/game/updatePauseScreen.c src/blob/AudPauseAllEffects.c src/blob/AudResumeAllEffects.c
 * game: src/blob/AudStopAllEffects.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* A StatLL header on the PC (shots.c's StatLL) with the n64 flag. */
typedef struct { uint8_t doubly, n64; int32_t count, size; void *mem, *active, *free; } StatLL;
/* InitBattlePowerUps's slot: the node, then its state. */
typedef struct { void *object; int16_t active, count; } Slot;

#define NF(n, off, T) (*(T *)((uint8_t *)(n) + (off)))   /* a node field at its N64 offset */
#define W32(p) ((uint32_t)(uintptr_t)(p))
#define WP(a) ((void *)(uintptr_t)(a))
#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data */

#define NODES TEST_N64(gTargetArray)  /* gTargetArray, 130 nodes of 0x70 bytes in the game arena */
#define NODE(i) ((uint8_t *)WP(NODES + 0x70u * (i)))
TEST_ABS(gTargetArray);
#define HEAP TEST_SCRATCH             /* the game heap (AllocateMemoryHeap, audio_dma_sync) */

StatLL gTargetList, gAlwaysColList;

int8_t gRestartGame, Restarting;
int32_t gPathCount, gBattleMineCnt, gRace, gBBoxModelCount, gSilverCoinCnt, gGoldCoinCnt, gWhichSilverCoin, gWhichGoldCoin;
uint8_t gTargetStructCount[8];
intptr_t gPlayerMines[4][5];
void *gAlwaysCol;
/* gBattlePowerUps, the State of InitBattlePowerUps and runtime_b_resource_callbacks. */
struct {
    int8_t count, selected_delay, selected_b, selected_a;
    float delay, phase_a, phase_b;
    Slot *slots;
} gBattlePowerUps;
volatile float gFrameSeconds = 0.5f;    /* gFrameSeconds */
/* The object definitions by kind: the power-ups' health (353), invisibility (354) and shield (359). */
uint16_t gObjList[360] = {[353] = 0x82, [354] = 0x84, [359] = 0x8E};
int16_t gNumPlayers;                  /* gNumPlayers: 0, so UpdateInvs leaves the cars alone */
struct pc_car pc_cars[PC_CAR_SLOTS];   /* the car records (pc_game_types.h) */

/* ---------------------------------------------------------------- what the code under test calls */

static uint32_t g_heap = HEAP;
static uint32_t g_slots_size;        /* the last block's size: InitBattlePowerUps' slots */
void *AllocateMemoryHeap(int32_t heap, int32_t size)
{
    void *p = WP(g_heap);
    g_slots_size = (uint32_t)size;
    memset(p, 0, (size_t)size);
    g_heap += (uint32_t)size;
    memset(WP(g_heap), 0xAB, 64);    /* a guard after each block */
    return p;
}
/* audio_dma_sync, the overlays' old name for AllocateMemoryHeap */
void *audio_dma_sync(void *heap, uint32_t size)
{
    return AllocateMemoryHeap(0, (int32_t)size);
}
float Random(float max) { return 0.0f; }   /* Random */
static int16_t g_def_index;
static uint16_t g_def;
void ZOID_SetObjectDef(int16_t index, uint16_t def) { g_def_index = index, g_def = def; }   /* ZOID_SetObjectDef */
static int32_t g_shown;
void MBOX_ShowObject(int32_t handle, int32_t a, uint32_t b) { g_shown = handle; }   /* MBOX_ShowObject */
void InitCollisionTargetPtrs(void) {}
void PreInitBattlePowerUps(void) {}
/* the pause screen (updatePauseScreen) */
int32_t gPauseState, pauseMenuSelect, playerInPause;
int16_t gNumWindows;
uint8_t gPlayers[4 * 80] __attribute__((aligned(16)));   /* the input records: edges at +4 */
volatile uint8_t gControllerData[4 * 16];
int8_t pause_gControllerRemoved, pause_gYes, gRetireGame, gSoftReset, controlScrActive[4];
float gModelTime;
uint32_t gGameState;
uint8_t audio_lock_queue[64], audio_command_queue[64];
static uint8_t g_audmsg[16];
void *GetCmd(void) { return g_audmsg; }
int32_t osRecvMesg(void *mq, void *msg, int32_t flag) { return 0; }
int32_t osSendMesg(void *mq, void *msg, int32_t flag) { return 0; }
void PauseGameMusic(void) {}           /* PauseGameMusic */
static int32_t g_pause_state = -1;
void setPauseState(int32_t state) { g_pause_state = state; }
void SaveServer(int32_t a, int32_t b) {}
void PauseTimer(void) {}
void ResumeTimer(void) {}
void StopListeners(void) {}
void modelstop(void) {}
void StartListeners(void) {}
void modelgo(void) {}
void StartGameMusic(void) {}
void ResumeGameMusic(void) {}
void SetControllerStickEdges(int32_t player, int32_t value) {}
void SaveFlush(int32_t action) {}
uint32_t AudPlayEffect(uint32_t fx, uint32_t sub, uint32_t replace, uint8_t prio) { return 0; }
void updateOptionScreen(void) {}
void updateControlScreen(void) {}
void updateAudioScreen(void) {}
void updateVideoScreen(void) {}
void PlayUDSound(int32_t edges) {}
void PlayLRSound(int32_t edges) {}
void PlayStartABSound(int32_t edges) {}
void AbortGame(void) {}
void pause_TitlePane(void) {}

/* ---------------------------------------------------------------- the game's functions */

void initTargetStructs(int32_t arg0);
void *GetStatLLE(void *pool);
void InitBattlePowerUps(int32_t reuse);
void UpdateBattleHealth(void);
void UpdateBattleShield(void);
void UpdateBattleInvs(void);
void UpdateInvs(void);            /* UpdateInvs: the invisible cars' fade */
void PauseTargets(void);
void updatePauseScreen(void);

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
    gRace = 0;
    initTargetStructs(0);
    CHECK(gTargetList.n64 == 1 && gTargetList.free == NODE(0));

    /* Five nodes on the active list (GetStatLLE puts each at its head: 4, 3, 2, 1, 0). Nodes 4, 3 and 1 are
     * power-ups (flag 0x20); every handle has an upper half, so only its low half is the scene node. */
    for (i = 0; i < 5; i++) {
        n[i] = GetStatLLE(&gTargetList);
        CHECK(n[i] == NODE(i));
        NF(n[i], 0x08, int32_t) = 0x4100 + i;                 /* key */
        NF(n[i], 0x0C, int32_t) = 0x00050100 + i;             /* scene-node handle */
        NF(n[i], 0x60, int32_t) = 7;                          /* sound */
    }
    CHECK(gTargetList.active == n[4] && NF(n[4], 0, uint32_t) == W32(n[3]) && NF(n[0], 0, uint32_t) == 0);
    NF(n[4], 4, uint8_t) = 0x20, NF(n[3], 4, uint8_t) = 0x28, NF(n[2], 4, uint8_t) = 0x01;
    NF(n[1], 4, uint8_t) = 0x20, NF(n[0], 4, uint8_t) = 0x10;
    memset(NODE(5), 0, 0x70);                                  /* a free node */

    /* The selection: one slot for each power-up, in list order, in a block of the slots' size. */
    gBattlePowerUps.count = 3;
    InitBattlePowerUps(0);
    s = gBattlePowerUps.slots;
    CHECK(g_slots_size >= 3 * sizeof(Slot));
    CHECK(guard_intact((uint8_t *)s + g_slots_size));
    CHECK(s[0].object == n[4] && s[1].object == n[3] && s[2].object == n[1]);
    for (i = 0; i < 3; i++)
        CHECK(s[i].active == 1 && s[i].count == 1);
    CHECK(gBattlePowerUps.delay == 5.0f && gBattlePowerUps.phase_a == 60.0f && gBattlePowerUps.phase_b == 15.0f);

    /* UpdateBattleHealth picks slot 0 (Random gives 0): node 4 is taken, gets resource 353 and the health
     * object definition on its scene node. */
    gBattlePowerUps.delay = 0.25f;
    UpdateBattleHealth();
    CHECK(gBattlePowerUps.selected_delay == 0 && s[0].active == 1 && s[0].count == 4 && gBattlePowerUps.delay == -2.0f);
    CHECK(NF(n[4], 4, uint8_t) == 0x22 && NF(n[4], 0x50, int16_t) == 353);
    CHECK(g_def_index == 0x104 && g_def == 0x82 && g_shown == 0x00050104);
    CHECK(NF(n[4], 0x08, int32_t) == 0x4104 && NF(n[4], 0x0C, int32_t) == 0x00050104);

    /* UpdateBattleShield goes past the taken slot to slot 1, node 3. */
    gBattlePowerUps.phase_b = 0.25f;
    UpdateBattleShield();
    CHECK(gBattlePowerUps.selected_b == 1 && s[1].count == 3 && gBattlePowerUps.phase_b == -2.0f);
    CHECK(NF(n[3], 4, uint8_t) == 0x2A && NF(n[3], 0x50, int16_t) == 359);
    CHECK(g_def_index == 0x103 && g_def == 0x8E && g_shown == 0x00050103);

    /* UpdateBattleInvs takes the last one, node 1. */
    gBattlePowerUps.phase_a = 0.25f;
    UpdateBattleInvs();
    CHECK(gBattlePowerUps.selected_a == 2 && s[2].count == 2 && gBattlePowerUps.phase_a == -2.0f);
    CHECK(NF(n[1], 4, uint8_t) == 0x22 && NF(n[1], 0x50, int16_t) == 354);
    CHECK(g_def_index == 0x101 && g_def == 0x84 && g_shown == 0x00050101);
    CHECK(NF(n[2], 4, uint8_t) == 0x01 && NF(n[0], 4, uint8_t) == 0x10);   /* not power-ups: untouched */

    /* PauseTargets: every node on the list, and only those, gets state 2 and sound -1. */
    PauseTargets();
    for (i = 0; i < 5; i++)
        CHECK(NF(n[i], 0x64, int8_t) == 2 && NF(n[i], 0x60, int32_t) == -1);
    CHECK(NF(NODE(5), 0x64, int8_t) == 0 && NF(NODE(5), 0x60, int32_t) == 0);
    CHECK(NF(n[4], 0, uint32_t) == W32(n[3]) && NF(n[4], 4, uint8_t) == 0x22);

    /* Player 0 pauses the race: PauseTargets does the same over the list. */
    for (i = 0; i < 5; i++)
        NF(n[i], 0x60, int32_t) = 7, NF(n[i], 0x64, int8_t) = 0;
    gPauseState = 0, gNumWindows = 1, gControllerData[0] = 1;
    NF(gPlayers, 4, int32_t) = 1;                            /* player 0's edges: start */
    updatePauseScreen();
    CHECK(g_pause_state == 1 && playerInPause == 0 && pause_gControllerRemoved == 0 && g_audmsg[2] == 7);
    for (i = 0; i < 5; i++)
        CHECK(NF(n[i], 0x64, int8_t) == 2 && NF(n[i], 0x60, int32_t) == -1);
    CHECK(NF(n[4], 0x68, uint32_t) == 0 && NF(NODE(5), 0x64, int8_t) == 0);

    /* UpdateInvs: an invisible car's fade out (phase 1) takes its u8 alpha (N64 +0x3A1) down 8 a 30 Hz frame until
     * it is 16 or less, then holds it at 16 (phase 3, +0x3A2). At original, from 24: 16. At game.frame_rate 60 a frame of
     * three 30 Hz frames (pc_frame_steps 3: five retraces with one left over) takes 24, and from 23 (a fade from 255
     * by 8s passes it) that went round to 255: a car seen whole again for most of a second. */
    {
        uint8_t *car = (uint8_t *)&pc_cars[0];
        static const struct { int fps60, steps, alpha, want_alpha, want_phase; } c[] = {
            {0, 1, 24, 16, 3}, {0, 1, 31, 23, 1}, {1, 1, 31, 23, 1}, {1, 3, 23, 16, 3}, {1, 3, 47, 23, 1},
            {1, 2, 16, 16, 3}, {1, 0, 23, 23, 1}};
        gNumPlayers = 1;
        for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
            NF(car, CAR_OFS(0x38C), uint32_t) |= 1;
            NF(car, CAR_OFS(0x3A2), int8_t) = 1;
            NF(car, CAR_OFS(0x3A1), uint8_t) = (uint8_t)c[i].alpha;
            NF(car, CAR_OFS(0x3A8), float) = 1.0f;
            pc_fps60 = c[i].fps60, pc_frame_steps = c[i].steps;
            UpdateInvs();
            CHECK(NF(car, CAR_OFS(0x3A1), uint8_t) == c[i].want_alpha &&
                  NF(car, CAR_OFS(0x3A2), int8_t) == c[i].want_phase);
        }
        pc_fps60 = 0, pc_frame_steps = 1;
        gNumPlayers = 0;
    }
    return TEST_DONE();
}
