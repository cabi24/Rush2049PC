/* NumCoinsFound: how many of a track's coins a player has found (MAT3 save.c). */
#include "game/game.h"

/* A player's input record as this reads it: which "just play" profile, and the profile handle in use. */
typedef struct PlayerPad {u8 first;u8 selector;u8 opaque[70];tProfile **profile;} PlayerPad;

extern PlayerPad gPlayers[];

/* The number of the current track's coins player has found (of its 16 bits: lane 1 the high 8 only, lane 2 the low
 * 8, 0 all), from its profile; a player without one is given its "just play" profile here, for good. 16 for a
 * profile whose file isn't read on a race track, 0 on tracks without coins. */
u8 NumCoinsFound(int index,int lane)
{
    int track=gTrackNum;
    u32 bits;
    if(track>=0 && track<6) {
        PlayerPad *player=&gPlayers[index];
        tSaveData **datah;
        if(player->profile==0)player->profile=&gJustPlayers[player->selector];
        datah=(*player->profile)->datah;
        if(datah==0)return 16;
        bits=(*datah)->raceStats[track].coinsFound;
    } else if(track>=TRACK_STUNT_FIRST && track<18) {
        PlayerPad *player=&gPlayers[index];
        if(player->profile==0)player->profile=&gJustPlayers[player->selector];
        bits=(*(*player->profile)->datah)->stuntStats[track-TRACK_STUNT_FIRST].coinsFound;
    } else bits=0;
    if(lane!=0) {
        u32 mask;
        if(lane==1)mask=0xff00;
        else mask=0xff;
        bits&=mask;
    }
    return (u8)CountBits(bits,16);
}
