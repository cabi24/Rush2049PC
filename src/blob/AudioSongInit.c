/* AudioSongInit: the N64 form of MAT3's audiosongps2.c start-up of the song player. */
#include "game/game.h"

/* No song playing: song_handle is the MusyX sequence of the playing song, -1 for none (AudioSongPlay). */
void AudioSongInit(void) {
    song_handle = -1;
}
