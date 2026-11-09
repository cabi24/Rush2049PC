/* RecordObstacle: record an obstacle course run (MAT3 gamerecords.c). */
#include "game/game.h"

/* A player's input record as this reads it (MAT3 tPadState): the car, which "just play" profile, the crashes, the
 * profile handle in use. */
typedef struct PlayerPad {u8 index,selector;u8 opaque[62];u16 numCrashes;u8 rest[6];tProfile **profile;} PlayerPad;
/* A car's record as this reads it: its race time. */
typedef struct CarTime {u8 opaque[240];f32 time;u8 rest[0x110 - 0xF4]; struct pc_visual visuals[21]; u8 rest_308[0x380 - 0x308]; void *input; u8 rest_384[0x3B8 - 0x384];} CarTime;

extern PlayerPad gPlayers[];

/* Adds each player's run to its profile's obstacle stats and to the totals: the time, a race, a finish (one lap) and
 * the best finishing time, the crashes, and the death mode runs; then writes the profile's stats (SavePlayerStats).
 * A player without a profile uses its "just play" one; a profile whose file isn't read stops it all. */
void RecordObstacle(void) {
 int player,which,course=gTrackNum-TRACK_OBSTACLE;
 PlayerPad *record;
 tObstacleStats *stats;
 CarTime *car;
 for(player=0;player<gNumPlayers;player++) {
  record=&gPlayers[player];
  if(record->profile==0)record->profile=&gJustPlayers[record->selector];
  if((*record->profile)->datah==0)return;
  for(which=0;which<2;which++) {
   if(which==0)stats=&(*(*record->profile)->datah)->obstacleStats[course];
   else stats=&gTotalObstacleStats[course];
   car=PC_CAR(record->index);
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
  SavePlayerStats(record->profile,(u8)gTrackNum);
 }
}
