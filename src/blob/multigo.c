/* multigo.c: let the simulation run one car (MAT3 mdrive.c, multigo()). */
#include "game/game.h"

/* Set car `slot`'s fastin.modelrun, so that the model thread steps it again (after its set-up or a respawn). */
void multigo(s16 slot) {
    pc_models[slot].fastin.modelrun = 1;
}
