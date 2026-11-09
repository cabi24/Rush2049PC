/* stop_current_song: stops the song playing (the sound system's AudioSongStop). */
#include "game/game.h"

/* Stops the song playing (AudioSongStop: the N64 stops whatever song_handle plays; MAT3 passes the song's number). */
void stop_current_song(void)
{
  AudioSongStop();
}
