/* RecordObstacle: record an obstacle course run (MAT3 gamerecords.c). */
#include "game/game.h"

/* A car's record as this reads it: its race time. */
typedef struct CarTime {u8 opaque[240];f32 time;u8 rest[0x110 - 0xF4]; struct pc_visual visuals[21]; u8 rest_308[0x380 - 0x308]; void *input; u8 rest_384[0x3B8 - 0x384];} CarTime;

/* Adds each player's run to its profile's obstacle stats and to the totals: the time, a race, a finish (one lap) and
 * the best finishing time, the crashes, and the death mode runs; then writes the profile's stats (SavePlayerStats).
 * A player without a profile uses its "just play" one; a profile whose file isn't read stops it all. */
void RecordObstacle(void) {
 int player,which,course=gTrackNum-TRACK_OBSTACLE;
 tPadState *record;
 tObstacleStats *stats;
 CarTime *car;
 for(player=0;player<gNumPlayers;player++) {
  record=&gPlayers[player];
  if(record->save==0)record->save=&gJustPlayers[record->controller];
  if((*record->save)->datah==0)return;
  for(which=0;which<2;which++) {
   if(which==0)stats=&(*(*record->save)->datah)->obstacleStats[course];
   else stats=&gTotalObstacleStats[course];
   car=PC_CAR(record->slot);
   stats->totalTime+=car->time;
   stats->totalRaces++;
   if(gLap[player]==1) {
    stats->totalFinished++;
    if(stats->bestTime==0.0f || car->time<stats->bestTime)stats->bestTime=car->time;
   }
   stats->totalCrashes+=record->numCrashes;
   if(gDeathMode) {
    stats->numDeathAttempted++;
    if(gLap[player]==1)stats->numDeathCompleted++;
   }
  }
  SavePlayerStats(record->save,(u8)gTrackNum);
 }
}
