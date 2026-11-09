/* audio_prenmi: silence the sound before a reset (the N64's PRENMI handler). */
#include "game/game.h"

/* Called when the reset button is pressed (the console's PRENMI warning, half a second before the reset): freezes the
 * sound output so the reset makes no noise. */
void audio_prenmi(void) {
    sound_freeze_output();
}
