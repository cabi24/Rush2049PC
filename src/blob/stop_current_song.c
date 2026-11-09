/* stop_current_song: stops the song playing (the sound system's AudioSongStop). */
#include "game/game.h"

void AudioSongStop();

/* Stops song song_number. */
void stop_current_song(void)
{
  AudioSongStop(song_number);
}
