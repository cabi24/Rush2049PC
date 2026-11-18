/* RemovePlayerFile: a profile's or ghost's file is gone (MAT3 save.c). */
#include "game/game.h"

/* The pak file records' RemoveFile for profiles and ghosts (the file was deleted, or its pak taken out): the profile
 * whose file handle is fileh leaves the profile list, the players and the high score tables; else the ghost whose
 * file it is leaves the ghost list. */
void RemovePlayerFile(tPakHandle *fileh) {
 tProfile **profile;
 tGhostFile **ghost;
 int i;
 for(profile=gPlayerFiles.head;profile!=0;profile=(*profile)->next) {
  if((*profile)->fileh==fileh) {
   for(i=0;i<gNumPlayers;i++) {
    if(gPlayers[i].save==profile)gPlayers[i].save=0;
   }
   RemovePlayerScores((intptr_t)profile);   /* the tables keep their owners' handles as numbers */
   RemoveLLE(&gPlayerFiles,profile);
   return;
  }
 }
 for(ghost=gGhostFiles.head;ghost!=0;ghost=(*ghost)->ll) {
  if((*ghost)->fileh==fileh) {
   RemoveLLE(&gGhostFiles,ghost);
   *ghost=0;
   return;
  }
 }
}
