/* FoundCoin: a player found one of a track's coins (MAT3 save.c). */
#include "game/game.h"

/* A player's input record as this reads it: which "just play" profile, and the profile handle in use. */
typedef struct PlayerPad {u8 index,selector;u8 opaque[70];tProfile **profile;} PlayerPad;

extern PlayerPad gPlayers[];

/* Sets coin bit's bit in the player's profile (or its "just play" profile): on a race track in both of the track's
 * race stats, on a stunt arena in its stunt stats; a profile with a pak file writes it (SavePlayerCoins). Nothing in
 * attract mode or on other tracks. */
void FoundCoin(int player,int bit) {
 PlayerPad *record;
 u32 mask;
 if(gGameState&GS_DEMO)return;
 if(gTrackNum>=0 && gTrackNum<6) {
  record=&gPlayers[player];
  if(record->profile==0) {
   mask=1u<<bit;
   (*gJustPlayers[record->selector]->datah)->raceStats[gTrackNum].coinsFound|=mask;
   (*gJustPlayers[record->selector]->datah)->raceStats[gTrackNum+NUM_RACE_TRACKS].coinsFound|=mask;
  } else if((*record->profile)->datah!=0) {
   mask=1u<<bit;
   (*(*record->profile)->datah)->raceStats[gTrackNum].coinsFound|=mask;
   (*(*record->profile)->datah)->raceStats[gTrackNum+NUM_RACE_TRACKS].coinsFound|=mask;
   SavePlayerCoins(record->profile,(u8)gTrackNum);
  }
 } else if(gTrackNum>=TRACK_STUNT_FIRST && gTrackNum<18) {
  record=&gPlayers[player];
  if(record->profile==0) {
   (*gJustPlayers[record->selector]->datah)->stuntStats[gTrackNum-TRACK_STUNT_FIRST].coinsFound|=1u<<bit;
  } else {
   (*(*record->profile)->datah)->stuntStats[gTrackNum-TRACK_STUNT_FIRST].coinsFound|=1u<<bit;
   SavePlayerCoins(record->profile,(u8)gTrackNum);
  }
 }
}
