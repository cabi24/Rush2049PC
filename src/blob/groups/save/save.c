/* SaveInit (MAT3 save.c): the profiles and ghosts the game keeps in memory, made at start-up; and the linked lists'
 * InitLL and ClearLL (MAT3 ll.c; InsertLLE.c has the lists). */
#include "game/game.h"

typedef struct SaveData {
    char pad0[13];
    char name[2051];
} SaveData; /* 0x810 */

typedef struct SaveSlot {
    void *next; void *prev; void *fileh; char pad0_C[0x14 - 0xC];
    char name[24];
    SaveData **data;
} SaveSlot; /* 0x30 */

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

VIEW(SaveSlot *[4], gJustPlayers);
#define gJustPlayers gJustPlayers_view
VIEW(SaveSlot[4], gRealJustPlayers);
#define gRealJustPlayers gRealJustPlayers_view
VIEW(SaveData *[4], gJustPlayerData);
#define gJustPlayerData gJustPlayerData_view
VIEW(SaveData[4], gRealJustPlayerData);
#define gRealJustPlayerData gRealJustPlayerData_view

VIEW(ListHead, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view
VIEW(ListHead, gGhostFiles);
#define gGhostFiles gGhostFiles_view

VIEW(s8, gNumLimbo);
#define gNumLimbo gNumLimbo_view

VIEW(u8, gGlobalsSet);
#define gGlobalsSet gGlobalsSet_view

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
    PurgeableMemory(PC_N64A(gRealLimboGhosts), 0);
    memset(gRealLimboGhosts, 0, n * sizeof(Record));
    gLimboGhosts = AllocateMemoryHeap(0, n * sizeof(Record *));
    PurgeableMemory(PC_N64A(gLimboGhosts), 0);
    for (i = 0; i < n; i++) {
        gLimboGhosts[i] = &gRealLimboGhosts[i];
    }
    gGlobalsSet = 0;
    save_gInited = 1;
}
