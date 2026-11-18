/* FindGhosts (MAT3 highscorescr.c _FindGhosts): the state of a Controller Pak for saving a ghost, and the player's
 * ghost already on it, for the high score screen. */
#include "game/game.h"

/* Sets gGhostStatus[pak]: -4 no controller, -1 no pak the pak system has read, -2 a damaged pak, -3 a Rumble Pak
 * (the messages playerscr_draw shows for PAK_NO_CONTROLLER, PAK_NOT_INSERTED, PAK_DAMAGED and PAK_RUMBLE; MAT3
 * _FindGhosts asks PS_PakInserted and PS_PakDamaged too), else 0; in a ghost race (gRace 2) 1
 * or 2 when the pak holds the first player's ghost of this track and direction (2 when it is another of the
 * player's records: its name or ids differ), whose list node goes to gPakGhosts[pak]. */
void FindGhosts(s32 pak)
{
    tProfile *player;
    tGhostFile ***ghosth;
    tGhostFile **node;
    tGhostFile *ghost;

    player = *gPlayers[0].save;
    ghosth = &gPakGhosts[pak];
    *ghosth = 0;
    if (gControllerData[pak].attached == 0) {
        gGhostStatus[pak] = -4;
    } else if (PS_PakInserted(pak) == 0) {
        gGhostStatus[pak] = -1;
    } else if (PS_PakDamaged(pak) != 0) {
        gGhostStatus[pak] = -2;
    } else if (PS_PakRumble(pak) != 0) {
        gGhostStatus[pak] = -3;
    } else {
        gGhostStatus[pak] = 0;
        if (gRace == kGhost) {
            for (node = gGhostFiles.head; node != 0; node = ghost->next) {
                ghost = *node;
                if (ghost->fileh != 0 && ghost->fileh->file->pak == pak && ghost->track == gTrackNum &&
                    ghost->backward == gBackwards &&
                    game_strncmp(ghost->fileh->file->name, player->fileh->file->name, 14) == 0) {
                    *ghosth = node;
                    if (game_strcmp(ghost->name, player->name) != 0 || player->id[0] != ghost->playerId[0] ||
                        player->id[1] != ghost->playerId[1]) {
                        gGhostStatus[pak] = 2;
                    } else {
                        gGhostStatus[pak] = 1;
                    }
                }
            }
        }
    }
}
