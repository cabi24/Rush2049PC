/* AudioShutdownSys: the N64 form of MAT3's audiosysps2.c shut-down of the sound system (empty in MAT3). */
#include "game/game.h"

/* Declared without a prototype, as the file always had it: the call passes song_number, which AudioSongStop
 * doesn't take (it stops whatever song_handle plays). */
void AudioSongStop();

/* Stops the sound server, the song and every MusyX voice, then waits until MusyX has no voice left
 * (AudioShutdown, before the sound manager's memory is freed). */
void AudioShutdownSys(void)
{
  audio_sfx_shutdown();
  AudioSongStop(song_number);
  sound_stop_everything();
  if (sound_is_idle() == 0)
  {
    do
    { pc_os_spin_yield("AudioShutdownSys");
    } while (sound_is_idle() == 0);
  }
}
