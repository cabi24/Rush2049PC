/* SetDefaultControls (MAT3 controls.c SetDefaultControls2, the N64's layout): the default control layout. */
#include "game/game.h"

/* Writes the default layout into controls[10]: for each control (CONTROL_GAS ...), the gControlTable index of the
 * button or axis that drives it. MAT3's has 11 controls. */
void SetDefaultControls(s8 *controls) {
    controls[CONTROL_GAS] = 1;
    controls[CONTROL_BRAKE] = 2;
    controls[CONTROL_STEER] = 0x13;
    controls[CONTROL_SHIFT_UP] = 9;
    controls[CONTROL_SHIFT_DOWN] = 0xF;
    controls[CONTROL_REVERSE] = 0x10;
    controls[CONTROL_ABORT] = 0xE;
    controls[7] = 8;
    controls[8] = 0xD;
    controls[CONTROL_WINGS] = 7;
}
