/* IsCoinFound: whether a player has found one of a track's coins (MAT3 save.c). */
#include "game/game.h"

/* A player's input record as this reads it: which "just play" profile, and the profile handle in use. */
typedef struct PlayerPad {u8 pad0;u8 index;u8 pad2[70];tProfile **profile;} PlayerPad;
extern PlayerPad gPlayers[];VIEW(tProfile *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view

/* Whether player has found coin number bit of the current track (the coins found bits of its race or stunt stats),
 * from the player's profile or else its "just play" profile. 1 (found) in attract mode, on other tracks, and for a
 * profile whose file is not read on a race track. */
u32 IsCoinFound(s32 player,s32 bit) {
 s32 selector;PlayerPad *p;tProfile **profile;tSaveData **datah;
 if(gGameState&GS_DEMO)return 1;
 selector=gTrackNum;
 if(selector>=0&&selector<6) {
  p=&gPlayers[player];profile=p->profile;
  if(!profile){return (*gJustPlayers[p->index]->datah)->raceStats[selector].coinsFound&(1u<<bit);}
  datah=(*profile)->datah;if(!datah)return 1;
  return (*datah)->raceStats[selector].coinsFound&(1u<<bit);
 }
 if(selector>=TRACK_STUNT_FIRST&&selector<18) {
  p=&gPlayers[player];profile=p->profile;
  if(!profile){return (*gJustPlayers[p->index]->datah)->stuntStats[selector-TRACK_STUNT_FIRST].coinsFound&(1u<<bit);}
  return (*(*profile)->datah)->stuntStats[selector-TRACK_STUNT_FIRST].coinsFound&(1u<<bit);
 }
 return 1;
}
