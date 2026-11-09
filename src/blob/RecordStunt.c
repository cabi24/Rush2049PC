/* RecordStunt: record a stunt session (MAT3 gamerecords.c). */
#include "game/game.h"

/* A player's input record as this reads it (MAT3 tPadState): which "just play" profile, the crashes, the profile. */
typedef struct PlayerPad {u8 index,selector;u8 other2[62];u16 numCrashes;u8 other66[6];tProfile **profile;} PlayerPad;
/* A player's stunt session as this reads it (MAT3 tStuntPlayer): the best stunt, the points, each kind's count. */
typedef struct StuntPlayer {u8 other0[8];u32 maxStuntPoints;u8 other12[8];s32 totalPoints;u8 other24[48];s16 totalStunts[10];u8 other92[28];} StuntPlayer;
extern PlayerPad gPlayers[];

VIEW(tProfile *[], gJustPlayers);
#define gJustPlayers gJustPlayers_view
VIEW(StuntPlayer[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view

/* Adds each player's session to its profile's stunt stats for the arena and to the totals: the best stunt, the best
 * points a minute, the points, the minutes, the crashes and each kind of stunt; then writes the profile's stats. A
 * player without a profile uses its "just play" one; a profile whose file isn't read stops it all. */
void RecordStunt(void)
{
    s32 player,which,j;
    s32 arena=gTrackNum-TRACK_STUNT_FIRST;
    PlayerPad *record;
    StuntPlayer *live;
    tStuntStats *stats;
    u32 rate;
    for(player=0;player<gNumPlayers;player++) {
        record=&gPlayers[player];
        if(record->profile==0) record->profile=&gJustPlayers[record->selector];
        if((*record->profile)->datah==0) return;
        live=&gStuntPlayer[player];
        for(which=0;which<2;which++) {
            if(which==0) stats=&(*(*record->profile)->datah)->stuntStats[arena];
            else stats=&gTotalStuntStats[arena];
            if(stats->bestStunt<live->maxStuntPoints) stats->bestStunt=live->maxStuntPoints;
            rate=live->totalPoints/gStuntModeMins;
            if(stats->bestAvg<rate) stats->bestAvg=rate;
            stats->totalPoints+=live->totalPoints;
            stats->totalTime+=gStuntModeMins;
            stats->totalCrashes+=record->numCrashes;
            for(j=0;j<10;j++) stats->totalStunts[j]+=live->totalStunts[j];
        }
        SavePlayerStats(record->profile,(u8)gTrackNum);
    }
}
