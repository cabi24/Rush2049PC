/* N64 _StartGameMusic and its two actual public wrappers.
 * MAT3 audio.c supplies the one song local, conditional track-table selection
 * and genuine music-volume/Random calls. The N64 twelve-song limit, retry for
 * song five, nonempty resume wrapper and queued audio backend remain native.
 * MAT3 source status is reported 100% fuzzy, not a fresh strict acceptance.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
typedef struct { u8 pad0[12]; s8 music; s8 sfx; s8 track; s8 mono; u8 remaining[25]; } AudioCfg;
extern AudioCfg gOptions;
extern s32 sTrackSongs[19];
extern s8 gTrackNum;
extern void AudPlaySong(s32, s32);
f32 Random(f32);
void AudSetMusicVol(f32,f32);
void StartGameMusicInternal(s32);
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
