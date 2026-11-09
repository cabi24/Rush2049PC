/* StartGameMusic and ResumeGameMusic: the race's music (MAT3 audio.c's, around _StartGameMusic, which is
 * StartGameMusicInternal here: game_music/StartGameMusicInternal.c). */
#include "game/game.h"

/* Starts the track's song at a race's start (its volume faded up in a twentieth of a second). */
void StartGameMusic(void) {StartGameMusicInternal(0);}
/* Brings the music back after the pause screen (PauseGameMusic), fading it up over a second. (Empty in MAT3.) */
void ResumeGameMusic(void) {StartGameMusicInternal(1);}
