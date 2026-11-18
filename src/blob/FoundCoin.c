/* FoundCoin: a player found one of a track's coins (MAT3 save.c). */
#include "game/game.h"

/* Sets coin bit's bit in the player's profile (or its "just play" profile): on a race track in both of the track's
 * race stats, on a stunt arena in its stunt stats; a profile with a pak file writes it (SavePlayerCoins). Nothing in
 * attract mode or on other tracks. */
void FoundCoin(int player,int bit) {
 tPadState *record;
 u32 mask;
 if(gGameState&GS_DEMO)return;
 if(gTrackNum>=0 && gTrackNum<6) {
  record=&gPlayers[player];
  if(record->save==0) {
   mask=1u<<bit;
   (*gJustPlayers[record->controller]->datah)->raceStats[gTrackNum].coinsFound|=mask;
   (*gJustPlayers[record->controller]->datah)->raceStats[gTrackNum+NUM_RACE_TRACKS].coinsFound|=mask;
  } else if((*record->save)->datah!=0) {
   mask=1u<<bit;
   (*(*record->save)->datah)->raceStats[gTrackNum].coinsFound|=mask;
   (*(*record->save)->datah)->raceStats[gTrackNum+NUM_RACE_TRACKS].coinsFound|=mask;
   SavePlayerCoins(record->save,(u8)gTrackNum);
  }
 } else if(gTrackNum>=TRACK_STUNT_FIRST && gTrackNum<18) {
  record=&gPlayers[player];
  if(record->save==0) {
   (*gJustPlayers[record->controller]->datah)->stuntStats[gTrackNum-TRACK_STUNT_FIRST].coinsFound|=1u<<bit;
  } else {
   (*(*record->save)->datah)->stuntStats[gTrackNum-TRACK_STUNT_FIRST].coinsFound|=1u<<bit;
   SavePlayerCoins(record->save,(u8)gTrackNum);
  }
 }
}
