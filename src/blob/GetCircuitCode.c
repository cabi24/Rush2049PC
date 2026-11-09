/* GetCircuitCode: the code that continues a player's circuit later (MAT3 circuit.c). */
#include "game/game.h"

/* Writes the circuit player's circuit code into `code`: the password (86 bits in 14 characters) of the circuit's
 * type, death option, races driven (the race in progress not counted: gCircuitTrack is set back), seed and
 * difficulty, and the six cars' points (SetCircuitCode reads it). */
void GetCircuitCode(u8 *code)
{
    s32 i;
    tCircuitRecord *circuit;
    circuit = &(*(*gPlayers_save[gCircuitPlayer].profile)->datah)->circuit;
    ContinueCircuit();
    gCircuitTrack -= 1;
    NewPassword(0x56U, 0xEU);
    StorePasswordElement((u32) circuit->type, 3U);
    StorePasswordElement((u32) circuit->death, 1U);
    StorePasswordElement((u32) circuit->track, 5U);
    StorePasswordElement(circuit->seed, 20U);
    StorePasswordElement((u32) circuit->difficulty, 3U);
    for (i = 0; i < 6; i++) {
        StorePasswordElement((u32) gCircuitCars[i].totalPoints, 9U);
    }
    GetPasswordString((char *) code);
}
