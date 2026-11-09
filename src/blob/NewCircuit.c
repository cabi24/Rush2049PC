/* NewCircuit: a player starts a circuit (MAT3 circuit.c). */
#include "game/game.h"


extern void *memset(void *s, s32 c, u32 n);



/* Starts a circuit of type `type` for the circuit player: their circuit record cleared and written to their pak
 * file (PS_WriteFile), active, its seed from the clock, the death option from the options, its difficulty the
 * type's. */
void NewCircuit(s32 type) {
    tCircuitRecord *circuit;

    circuit = &(*(*gPlayers_save[gCircuitPlayer].profile)->datah)->circuit;
    memset(circuit, 0, sizeof(tCircuitRecord));
    PS_WriteFile((void **) (*gPlayers_save[gCircuitPlayer].profile)->fileh, (u8 *) circuit, sizeof(tCircuitRecord));
    circuit->type = (s8) type;
    circuit->seed = *(s32 *) &gSeconds & 0xFFFFF;   /* the game time's float bits (MAT3: *(unsigned int *)&gSeconds) */
    circuit->active = 1;
    circuit->totalTime = 0.0f;
    if (gOptions[kOptDeathMode] != 0) {
        circuit->death = 1;
    }
    circuit->difficulty = (s8) gCircuitDiff[type];
}
