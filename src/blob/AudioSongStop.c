#include "game/game.h"

M2C_UNK sound_seq_stop(s32, s32);
void AudioSongStop(void)
{
  s32 new_var;
  if (song_handle != (-1))
  {
    new_var = song_handle;
    sound_seq_stop(new_var, song_handle);
    sound_pop_group();
    UnloadWaveInstance(song_wave);
    song_handle = -1;
  }
}
