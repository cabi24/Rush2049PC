/* IsCoinFound: whether a player has found one of a track's coins (MAT3 save.c). */
#include "game/game.h"

/* Whether player has found coin number bit of the current track (the coins found bits of its race or stunt stats),
 * from the player's profile or else its "just play" profile. 1 (found) in attract mode, on other tracks, and for a
 * profile whose file is not read on a race track. */
u32 IsCoinFound(s32 player,s32 bit) {
 s32 selector;tPadState *p;tProfile **profile;tSaveData **datah;
 if(gGameState&GS_DEMO)return 1;
 selector=gTrackNum;
 if(selector>=0&&selector<6) {
  p=&gPlayers[player];profile=p->save;
  if(!profile){return (*gJustPlayers[p->controller]->datah)->raceStats[selector].coinsFound&(1u<<bit);}
  datah=(*profile)->datah;if(!datah)return 1;
  return (*datah)->raceStats[selector].coinsFound&(1u<<bit);
 }
 if(selector>=TRACK_STUNT_FIRST&&selector<18) {
  p=&gPlayers[player];profile=p->save;
  if(!profile){return (*gJustPlayers[p->controller]->datah)->stuntStats[selector-TRACK_STUNT_FIRST].coinsFound&(1u<<bit);}
  return (*(*profile)->datah)->stuntStats[selector-TRACK_STUNT_FIRST].coinsFound&(1u<<bit);
 }
 return 1;
}
