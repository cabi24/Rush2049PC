/* RecordObstacle: record an obstacle course run (MAT3 gamerecords.c). */
#include "game/game.h"

/* A player's input record as this reads it (MAT3 tPadState): the car, which "just play" profile, the crashes, the
 * profile handle in use. */
typedef struct PlayerPad {u8 index,selector;u8 opaque[62];u16 numCrashes;u8 rest[6];tProfile **profile;} PlayerPad;

extern PlayerPad gPlayers[];

/* Adds each player's run to its profile's obstacle stats and to the totals: the time, a race, a finish (one lap) and
 * the best finishing time, the crashes, and the death mode runs; then writes the profile's stats (SavePlayerStats).
 * A player without a profile uses its "just play" one; a profile whose file isn't read stops it all. */
void RecordObstacle(void) {
 int player,which,course=gTrackNum-TRACK_OBSTACLE;
 PlayerPad *record;
 tObstacleStats *stats;
 tCarData *car;
 for(player=0;player<gNumPlayers;player++) {
  record=&gPlayers[player];
  if(record->profile==0)record->profile=&gJustPlayers[record->selector];
  if((*record->profile)->datah==0)return;
  for(which=0;which<2;which++) {
   if(which==0)stats=&(*(*record->profile)->datah)->obstacleStats[course];
   else stats=&gTotalObstacleStats[course];
   car=&pc_cars[record->index];
   stats->totalTime+=car->score;
   stats->totalRaces++;
   if(gLap[player]==1) {
    stats->totalFinished++;
    if(stats->bestTime==0.0f || car->score<stats->bestTime)stats->bestTime=car->score;
   }
   stats->totalCrashes+=record->numCrashes;
   if(gDeathMode) {
    stats->numDeathAttempted++;
    if(gLap[player]==1)stats->numDeathCompleted++;
   }
  }
  SavePlayerStats(record->profile,(u8)gTrackNum);
 }
}
