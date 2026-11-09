/* SaveInit (MAT3 save.c): the profiles and ghosts the game keeps in memory, made at start-up; and the linked lists'
 * InitLL and ClearLL (MAT3 ll.c; InsertLLE.c has the lists). */
#include "game/game.h"

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)

typedef struct ListHead {   /* a list's header (MAT3 tDLList), the PC's layout */
    s8 handle;      /* the list links handles, not records */
    s8 dbl;         /* doubly linked */
    s32 numElements;
    intptr_t head;
    intptr_t tail;
} ListHead;

typedef struct Record {     /* a ghost (SaveInit' Ghost), 64 bytes on the PC (docs/port/POINTERS.md 9.10) */
    void *ll;
    void **fileh;
    char pad08[0x24 - 0x08];
    void **datah;
    void **header;
} Record;

s32 sprintf(char *, const char *, ...);

void *memset(void *, s32, u32);

/* Empties list (the records are the caller's). */
void ClearLL(ListHead *list) {
    list->head = 0;
    list->tail = 0;
    list->numElements = 0;
}

/* Makes list empty: of handles (handle) or records, doubly linked (dbl) or not. */
void InitLL(ListHead *list, s8 handle, s8 dbl) {
    list->handle = handle;
    list->dbl = dbl;
    ClearLL(list);
}

/* The four controllers' own profiles (gJustPlayers: "Player n", for a player without a pak), the lists of the
 * profiles and ghosts on the paks, and the ghosts kept in memory ("limbo": one race's three, or twelve races' with
 * an Expansion Pak), locked in the heap. */
void SaveInit(void) {
    s32 i;
    s32 n;

    for (i = 0; i < 4; i++) {
        gJustPlayers[i] = &gRealJustPlayers[i];
        gRealJustPlayers[i].datah = &gJustPlayerData[i];
        gJustPlayerData[i] = &gRealJustPlayerData[i];
        InitPlayer(&gJustPlayers[i]);
        sprintf(gJustPlayers[i]->name, player_name_format, i + 1);
        game_strcpy((*gJustPlayers[i]->datah)->name, gJustPlayers[i]->name);
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
