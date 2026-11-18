/* RecordCircuit: record a finished circuit (MAT3 gamerecords.c). */
#include "game/game.h"


/* Adds the circuit the circuit player (gCircuitPlayer) finished to its profile's stats for that circuit (the type of
 * its circuit in progress) and to the totals: a circuit, a first, second or third place, the death mode attempts and
 * completions (a place in the top three), the points and the best points, the best time; then writes the stats. A
 * player without a profile uses its "just play" one. */
void RecordCircuit(void) {
    s32 i;
    s32 circuit;
    tCircuitStats *rec;

    if (gPlayers[gCircuitPlayer].save == 0) {
        gPlayers[gCircuitPlayer].save = &gJustPlayers[gPlayers[gCircuitPlayer].controller];
    }
    circuit = (*(*gPlayers[gCircuitPlayer].save)->datah)->circuit.type;
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            rec = &(*(*gPlayers[gCircuitPlayer].save)->datah)->circuitStats[circuit];
        } else {
            rec = &gTotalCircuitStats[circuit];
        }
        rec->numCircuits++;
        if (gCircuitCars[0].totalPlace == 0) {
            rec->num1st++;
        } else if (gCircuitCars[0].totalPlace == 1) {
            rec->num2nd++;
        } else if (gCircuitCars[0].totalPlace == 2) {
            rec->num3rd++;
        }
        if (gDeathMode) {
            rec->numDeathAttempted++;
            if (gCircuitCars[0].totalPlace < 3) {
                rec->numDeathCompleted++;
            }
        }
        rec->totalPts += gCircuitCars[0].totalPoints;
        if (rec->bestPts < gCircuitCars[0].totalPoints) {
            rec->bestPts = gCircuitCars[0].totalPoints;
        }
        if (rec->bestTime == 0.0f || (*(*gPlayers[gCircuitPlayer].save)->datah)->circuit.totalTime < rec->bestTime) {
            rec->bestTime = (*(*gPlayers[gCircuitPlayer].save)->datah)->circuit.totalTime;
        }
    }
    SavePlayerCircuitStats(gPlayers[gCircuitPlayer].save, (*(*gPlayers[gCircuitPlayer].save)->datah)->circuit.type);
}
