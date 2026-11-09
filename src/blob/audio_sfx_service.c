/* audio_sfx_service: the sound effect server's service, as a function of its own. */
#include "game/game.h"

/* Runs the sound effect server once (AudioSfxServer: its commands and voices). */
void audio_sfx_service(void) {
    AudioSfxServer();
}
