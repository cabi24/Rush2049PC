/* UpdateCircuit: a circuit after one of its races (MAT3 circuit.c). */
#include "game/game.h"

#define SWAP(a, b) { (a) ^= (b); (b) ^= (a); (a) ^= (b); }

/* Records the race just driven: each car's place (`values`, 3 bits each, the two players' swapped for the second
 * player's profile) into the circuit's ranks, `time` added to its time; the next race is set up (ContinueCircuit),
 * and the circuit recorded once done. A full circuit (type 3) gets harder after a good race (by the best player's
 * place and the leader's margin in points). The profile is saved. */
void UpdateCircuit(u8 *values, float time)
{
    tSaveData *data;
    tCircuit *circuit;
    int count, i, j, bit, best;
    int first, second, difference, level;

    data = *(*gPlayers[gCircuitPlayer].save)->datah;
    circuit = &data->circuit;
    count = 6 - (gNumPlayers == 1 ? 0 : 2);
    if (gCircuitPlayer > 0) {
        SWAP(values[0], values[1]);
    }
    for (i = 0; i < count; i++) {
        bit = gCircuitTrack * 3;
        for (j = 0; j < 3; j++) {
            circuit->ranks[i][bit >> 3] =
                (circuit->ranks[i][bit >> 3] & ~(1 << (bit & 7))) |
                ((values[i] & 1) << (bit & 7));
            values[i] >>= 1;
            bit++;
        }
    }
    circuit->totalTime += time;
    circuit->track++;
    ContinueCircuit();
    gCircuitTrack--;
    if (circuit->track >= gCircuitSize) RecordCircuit();
    if (circuit->type == 3) {
        if (gNumPlayers == 1) best = gCircuitCars[0].totalPlace;
        else best = gCircuitCars[0].totalPlace < gCircuitCars[1].totalPlace ? gCircuitCars[0].totalPlace : gCircuitCars[1].totalPlace;
        for (i = 0; i < 6; i++) {
            if (gCircuitCars[i].totalPlace == 0) first = i;
            else if (gCircuitCars[i].totalPlace == 1) second = i;
        }
        difference = gCircuitCars[first].totalPoints - gCircuitCars[second].totalPoints;
        level = circuit->difficulty;
        if ((best == 0 && level < 5 && difference >= 20) ||
            (best == 0 && level < 4 && difference >= 6) ||
            (best < 2 && level < 3) ||
            (best < 3 && level < 2) ||
            (best < 4 && level <= 0)) circuit->difficulty = level + 1;
    }
    SavePlayerCircuit(gPlayers[gCircuitPlayer].save);
}
