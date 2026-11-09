/* game_music/StartGameMusicInternal.c: start a race's music (MAT3 audio.c, _StartGameMusic()). */
#include "game/game.h"


/* Start the race's music, if the music volume is not 0: the track's song (sTrackSongs[gTrackNum]; -1 a random one
 * of the first 12 but song 5) when the music option is 12 (the track's own), the song the option names when it is
 * below 12. The volume is the option's tenth, set with AudSetMusicVol's second argument 1.0 when `resume` (back from
 * the pause), else 0.05. */
void StartGameMusicInternal(s32 resume)
{
 s32 song;
 if(gOptions[kOptMusicVolume]) {
  if(sTrackSongs[gTrackNum] == -1) {
   do { song=(s32)Random(12.0f); } while(song == 5);
  } else song=sTrackSongs[gTrackNum];
  AudSetMusicVol(gOptions[kOptMusicVolume] / 10.0f,resume ? 1.0f : 0.05f);
  if(gOptions[kOptSong] == 12) AudPlaySong(song,1);
  else if(gOptions[kOptSong] < 12) AudPlaySong(gOptions[kOptSong],1);
 }
}
