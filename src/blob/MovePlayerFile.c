/* MovePlayerFile: a profile's or ghost's file got a new handle (MAT3 save.c). */
#include "game/game.h"

typedef struct { u8 indirect, doubly, unknown02[2]; s32 count; tGhostFile **first, **last; } GhostList;
VIEW(GhostList, gGhostFiles);
#define gGhostFiles gGhostFiles_view

/* The pak file records' MoveFile for profiles and ghosts: the profile, else the ghost, whose file handle is from now
 * has the handle to (PS_PakServer moves a pak's files to another port). */
void MovePlayerFile(tPakHandle *from, tPakHandle *to) {
    tProfile **profile;
    tGhostFile **ghost;

    for (profile = gPlayerFiles.head; profile != NULL; profile = (*profile)->next) {
        if ((*profile)->fileh == from) {
            (*profile)->fileh = to;
            return;
        }
    }
    for (ghost = gGhostFiles.first; ghost != NULL; ghost = (*ghost)->ll) {
        if ((*ghost)->fileh == from) {
            (*ghost)->fileh = to;
            return;
        }
    }
}
