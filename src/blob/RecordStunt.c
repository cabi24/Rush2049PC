/* RecordStunt: record a stunt session (MAT3 gamerecords.c). */
#include "game/game.h"

/* A player's stunt session as this reads it (MAT3 tStuntPlayer): the best stunt, the points, each kind's count. */
typedef struct StuntPlayer {u8 other0[8];u32 maxStuntPoints;u8 other12[8];s32 totalPoints;u8 other24[48];s16 totalStunts[10];u8 other92[28];} StuntPlayer;

/* Adds each player's session to its profile's stunt stats for the arena and to the totals: the best stunt, the best
 * points a minute, the points, the minutes, the crashes and each kind of stunt; then writes the profile's stats. A
 * player without a profile uses its "just play" one; a profile whose file isn't read stops it all. */
void RecordStunt(void)
{
    s32 player,which,j;
    s32 arena=gTrackNum-TRACK_STUNT_FIRST;
    tPadState *record;
    StuntPlayer *live;
    tStuntStats *stats;
    u32 rate;
    for(player=0;player<gNumPlayers;player++) {
        record=&gPlayers[player];
        if(record->save==0) record->save=&gJustPlayers[record->controller];
        if((*record->save)->datah==0) return;
        live=&gStuntPlayer[player];
        for(which=0;which<2;which++) {
            if(which==0) stats=&(*(*record->save)->datah)->stuntStats[arena];
            else stats=&gTotalStuntStats[arena];
            if(stats->bestStunt<live->maxStuntPoints) stats->bestStunt=live->maxStuntPoints;
            rate=live->totalPoints/gStuntModeMins;
            if(stats->bestAvg<rate) stats->bestAvg=rate;
            stats->totalPoints+=live->totalPoints;
            stats->totalTime+=gStuntModeMins;
            stats->totalCrashes+=record->numCrashes;
            for(j=0;j<10;j++) stats->totalStunts[j]+=live->totalStunts[j];
        }
        SavePlayerStats(record->save,(u8)gTrackNum);
    }
}
