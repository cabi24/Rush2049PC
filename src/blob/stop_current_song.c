#include "game/game.h"

void AudioSongStop();

void stop_current_song(void)
{
  AudioSongStop(song_number);
}
