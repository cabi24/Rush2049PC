#include "game/game.h"

void AudioSongStop();

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
