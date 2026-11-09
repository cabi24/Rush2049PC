/* InitSoundState: empties a car's sound record (MAT3 audio.c's _InitSoundState). */
#include "game/game.h"

/* No sound (-1) and no values sent yet (AUDIO_NO_CHANGE, -2: the next SetSoundVP sends whatever it gets). */
void InitSoundState(tSoundState *sound) {
    sound->sound = -1;
    sound->vol = -2.0f;
    sound->pitch = -2.0f;
    sound->lrpan = -2.0f;
    sound->fbpan = -2.0f;
}
