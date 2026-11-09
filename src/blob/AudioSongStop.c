/* AudioSongStop: stops the song (the N64 form of MAT3's audiosongps2.c AudioSongStop). */
#include "game/game.h"

/* sound_seq_stop takes one argument (the sequence); this file has always passed the handle twice, which the
 * callee ignores (docs/code/README.md "The headers": callers that pass another number of arguments). */
M2C_UNK sound_seq_stop(s32, s32);

/* If a song plays: stops its MusyX sequence, unloads its sound group and its wave (the song data AudioSongPlay
 * loaded), and marks none playing. MAT3 takes the song's number; the N64 stops whatever plays. */
void AudioSongStop(void)
{
  s32 handle;
  if (song_handle != (-1))
  {
    handle = song_handle;
    sound_seq_stop(handle, song_handle);
    sound_pop_group();
    UnloadWaveInstance(song_wave);
    song_handle = -1;
  }
}
