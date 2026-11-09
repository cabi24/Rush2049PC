#include "game/game.h"

/* PC: the pak file record's and the profile's host layouts (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Q { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 id; u8 pad17;
    s8 name[14]; } Q;
typedef struct Ghost { struct Ghost **next; Q **q; s8 track; u8 backwards; s8 pos[14]; s32 id0; s32 id1; } Ghost;
typedef struct Player { void *w0, *w4; Q **qq; s32 id0; s32 id1; s8 name[4]; } Player;

typedef struct Slot { u8 pad0; s8 formatted; u8 pad2[3]; s8 damaged; s8 full; u8 rest[5]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } Slot;
typedef struct PakState { s8 inserted; u8 rest[15]; } PakState;

VIEW(Player **, gPlayers_save);
#define gPlayers_save gPlayers_save_view

VIEW(PakState[], gControllerData);
#define gControllerData gControllerData_view
VIEW(Slot[], gPaks);
#define gPaks gPaks_view

/* The ghost files on the paks (carscr.c's RecordList): first the list head. */
typedef struct RecordList {u8 indirect,doubly,unknown02[2];s32 count;Ghost **first, **last;} RecordList;
VIEW(RecordList, gGhostFiles);
#define gGhostFiles gGhostFiles_view

s32 pak_slot_b1(s32 pak) { return gPaks[pak].formatted; }
s32 pak_slot_b5(s32 pak) { return gPaks[pak].damaged; }
s32 pak_slot_b6(s32 pak) { return gPaks[pak].full; }

void FindGhosts(s32 pak)
{
    Player *player;
    Ghost ***ghosth;
    Ghost **node;
    Ghost *ghost;

    player = *gPlayers_save;
    ghosth = &gPakGhosts[pak];
    *ghosth = 0;
    if (gControllerData[pak].inserted == 0) {
        gGhostStatus[pak] = -4;
    } else if (pak_slot_b1(pak) == 0) {
        gGhostStatus[pak] = -1;
    } else if (pak_slot_b5(pak) != 0) {
        gGhostStatus[pak] = -2;
    } else if (pak_slot_b6(pak) != 0) {
        gGhostStatus[pak] = -3;
    } else {
        gGhostStatus[pak] = 0;
        if (gRace == 2) {
            do {} while (0);
            for (node = gGhostFiles.first; node != 0; node = ghost->next) {
                ghost = *node;
                if (ghost->q != 0 && (*ghost->q)->id == pak && ghost->track == gTrackNum &&
                    ghost->backwards == gBackwards &&
                    game_strncmp((*ghost->q)->name, (*player->qq)->name, 14) == 0) {
                    *ghosth = node;
                    if (game_strcmp(ghost->pos, player->name) != 0 || player->id0 != ghost->id0 ||
                        player->id1 != ghost->id1) {
                        gGhostStatus[pak] = 2;
                    } else {
                        gGhostStatus[pak] = 1;
                    }
                }
            }
        }
    }
}
