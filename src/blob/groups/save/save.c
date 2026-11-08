/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * SaveInit (historical label): save-slot table initialiser.
 *   - binds the four save slots (gJustPlayers[i] -> gRealJustPlayers[i] -> gJustPlayerData[i] -> gRealJustPlayerData[i]),
 *     resets each through InitPlayer and names it with sprintf(player_name_format, i + 1);
 *   - initialises two list heads with InitLL;
 *   - allocates n = (expansion_pak_present ? 12 : 1) * 3 44-byte records plus an index of pointers to them.
 * Whole-program facts this depends on (it cannot match alone):
 *   - InitPlayer is internal: its handle arrives in s5 and it leaves s0/s1/s2/s4/s6 unsaved, so this
 *     function saves every callee-saved register (and f20/f22) and spills s0/s1 around the call;
 *   - InitLL(list, a, b) is inlined twice, and it in turn inlines ClearLL(list). That nesting is
 *     what gives the 160-byte frame (24 bytes per inlined call) and the retail store order b0, b1, w8, wC, w4.
 *     Both helpers are redefined here in that natural form; each still produces its own locked body.
 * Quirk: gGlobalsSet / save_gInited must be unsigned char so the final `= 1` is not commoned with the s8 `1`
 * that the inlined InitLL arguments and gNumLimbo share in a1.
 * No arcade ancestor (N64 controller-pak / options code).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)

typedef struct SaveData {
    char pad0[13];
    char name[2051];
} SaveData; /* 0x810 */

typedef struct SaveSlot {
    void *next; void *prev; void *fileh; char pad0_C[0x14 - 0xC];
    char name[24];
    SaveData **data;
} SaveSlot; /* 0x30 */

typedef struct ListHead {   /* tDLList: the head and tail are pointers (RemovePlayerFile's List16) */
    s8 unk0;
    s8 unk1;
    s32 unk4;
    intptr_t unk8;
    intptr_t unkC;
} ListHead;

typedef struct Record {     /* a ghost (SaveInit' Ghost), 64 bytes on the PC (docs/port/POINTERS.md 9.10) */
    void *ll;
    void **fileh;
    char pad08[0x24 - 0x08];
    void **datah;
    void **header;
} Record;

extern SaveSlot *gJustPlayers[4];
extern SaveSlot gRealJustPlayers[4];
extern SaveData *gJustPlayerData[4];
extern SaveData gRealJustPlayerData[4];
extern char player_name_format[];
extern ListHead gPlayerFiles;
extern ListHead gGhostFiles;
extern s8 expansion_pak_present;
extern s8 gNumLimbo;
extern Record *gRealLimboGhosts;
extern Record **gLimboGhosts;
extern u8 gGlobalsSet;
extern u8 save_gInited;

void InitPlayer(SaveSlot **h);
s32 sprintf(char *, const char *, ...);
void game_strcpy(void *, char *);
void *AllocateMemoryHeap(s32 arg0, s32 arg1);
void PurgeableMemory(u32 arg0, s32 arg1);
void *memset(void *, s32, u32);

void ClearLL(ListHead *list) {
    list->unk8 = 0;
    list->unkC = 0;
    list->unk4 = 0;
}

void InitLL(ListHead *list, s8 a, s8 b) {
    list->unk0 = a;
    list->unk1 = b;
    ClearLL(list);
}

void SaveInit(void) {
    s32 i;
    s32 n;

    for (i = 0; i < 4; i++) {
        gJustPlayers[i] = &gRealJustPlayers[i];
        gRealJustPlayers[i].data = &gJustPlayerData[i];
        gJustPlayerData[i] = &gRealJustPlayerData[i];
        InitPlayer(&gJustPlayers[i]);
        sprintf(gJustPlayers[i]->name, player_name_format, i + 1);
        game_strcpy((*gJustPlayers[i]->data)->name, gJustPlayers[i]->name);
    }
    InitLL(&gPlayerFiles, 1, 1);
    InitLL(&gGhostFiles, 1, 0);
    if (expansion_pak_present == 0) {
        gNumLimbo = 1;
    } else {
        gNumLimbo = 3;
    }
    n = (expansion_pak_present ? 12 : 1) * 3;
    gRealLimboGhosts = AllocateMemoryHeap(0, n * sizeof(Record));
    PurgeableMemory(N64A(gRealLimboGhosts), 0);
    memset(gRealLimboGhosts, 0, n * sizeof(Record));
    gLimboGhosts = AllocateMemoryHeap(0, n * sizeof(Record *));
    PurgeableMemory(N64A(gLimboGhosts), 0);
    for (i = 0; i < n; i++) {
        gLimboGhosts[i] = &gRealLimboGhosts[i];
    }
    gGlobalsSet = 0;
    save_gInited = 1;
}
