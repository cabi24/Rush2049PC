/* FindGhosts (MAT3 highscorescr.c _FindGhosts): the state of a Controller Pak for saving a ghost, and the player's
 * ghost already on it, for the high score screen. */
#include "game/game.h"

/* PC: the pak file record's and the profile's host layouts (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Q { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 id; u8 pad17;
    s8 name[14]; } Q;
typedef struct Ghost { struct Ghost **next; Q **q; s8 track; u8 backwards; s8 pos[14]; s32 id0; s32 id1; } Ghost;
typedef struct Player { void *w0, *w4; Q **qq; s32 id0; s32 id1; s8 name[4]; } Player;

typedef struct PakState { s8 inserted; u8 rest[15]; } PakState;

VIEW(Player **, gPlayers_save);
#define gPlayers_save gPlayers_save_view

VIEW(PakState[], gControllerData);
#define gControllerData gControllerData_view

/* The ghost files on the paks (carscr.c's RecordList): first the list head. */
typedef struct RecordList {u8 indirect,doubly,unknown02[2];s32 count;Ghost **first, **last;} RecordList;
VIEW(RecordList, gGhostFiles);
#define gGhostFiles gGhostFiles_view

/* Sets gGhostStatus[pak]: -4 no controller, -1 no pak the pak system has read, -2 a damaged pak, -3 a Rumble Pak
 * (the messages playerscr_draw shows for PAK_NO_CONTROLLER, PAK_NOT_INSERTED, PAK_DAMAGED and PAK_RUMBLE; MAT3
 * _FindGhosts asks PS_PakInserted and PS_PakDamaged too), else 0; in a ghost race (gRace 2) 1
 * or 2 when the pak holds the first player's ghost of this track and direction (2 when it is another of the
 * player's records: its name or ids differ), whose list node goes to gPakGhosts[pak]. */
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
    } else if (PS_PakInserted(pak) == 0) {
        gGhostStatus[pak] = -1;
    } else if (PS_PakDamaged(pak) != 0) {
        gGhostStatus[pak] = -2;
    } else if (PS_PakRumble(pak) != 0) {
        gGhostStatus[pak] = -3;
    } else {
        gGhostStatus[pak] = 0;
        if (gRace == 2) {
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
