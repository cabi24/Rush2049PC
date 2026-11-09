/* RemovePlayerFile: a profile's or ghost's file is gone (MAT3 save.c). */
#include "game/game.h"

/* The profile and ghost lists (MAT3 tDLList). */
typedef struct ProfileList {u8 indirect,doubly;u8 gap[2];int count;tProfile **head,**tail;} ProfileList;
typedef struct GhostList {u8 indirect,doubly;u8 gap[2];int count;tGhostFile **head,**tail;} GhostList;
/* A player's input record as this reads it: the handle of the profile it uses. */
typedef struct PlayerPad {u8 opaque[72];tProfile **profile;} PlayerPad;
VIEW(ProfileList, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view
VIEW(GhostList, gGhostFiles);
#define gGhostFiles gGhostFiles_view
extern PlayerPad gPlayers[];

/* The pak file records' RemoveFile for profiles and ghosts (the file was deleted, or its pak taken out): the profile
 * whose file handle is fileh leaves the profile list, the players and the high score tables; else the ghost whose
 * file it is leaves the ghost list. (The handle is passed whole, as an integer.) */
void RemovePlayerFile(uintptr_t fileh) {
 tProfile **profile;
 tGhostFile **ghost;
 int i;
 for(profile=gPlayerFiles.head;profile!=0;profile=(*profile)->next) {
  if((uintptr_t)(*profile)->fileh==fileh) {
   for(i=0;i<gNumPlayers;i++) {
    if(gPlayers[i].profile==profile)gPlayers[i].profile=0;
   }
   RemovePlayerScores(profile);
   RemoveLLE(&gPlayerFiles,profile);
   return;
  }
 }
 for(ghost=gGhostFiles.head;ghost!=0;ghost=(*ghost)->ll) {
  if((uintptr_t)(*ghost)->fileh==fileh) {
   RemoveLLE(&gGhostFiles,ghost);
   *ghost=0;
   return;
  }
 }
}
