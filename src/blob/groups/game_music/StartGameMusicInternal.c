#include "game/game.h"

typedef struct { u8 pad0[12]; s8 music; s8 sfx; s8 track; s8 mono; u8 remaining[25]; } AudioCfg;
VIEW(AudioCfg, gOptions);
#define gOptions gOptions_view
VIEW(s32[19], sTrackSongs);
#define sTrackSongs sTrackSongs_view

void StartGameMusicInternal(s32 resume)
{
 s32 song;
 if(gOptions.music) {
  if(sTrackSongs[gTrackNum] == -1) {
   do { song=(s32)Random(12.0f); } while(song == 5);
  } else song=sTrackSongs[gTrackNum];
  AudSetMusicVol(gOptions.music / 10.0f,resume ? 1.0f : 0.05f);
  if(gOptions.track == 12) AudPlaySong(song,1);
  else if(gOptions.track < 12) AudPlaySong(gOptions.track,1);
 }
}
