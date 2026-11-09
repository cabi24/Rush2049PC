/* ValidateEmitter: the sound manager's emitter check, empty in the shipped game (MAT3 audiomgr.c's
 * _ValidateEmitter). audiomgr_emitters/group.c calls its own empty copy, pc_ValidateEmitter. */
#include "game/game.h"

/* A debug build's sanity check of an emitter; does nothing. */
void ValidateEmitter(void) {

}
