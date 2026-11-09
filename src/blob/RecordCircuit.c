/* RecordCircuit: record a finished circuit (MAT3 gamerecords.c). */
#include "game/game.h"

/* A player's input record as this reads it: which "just play" profile, the profile handle in use. */
typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 index;
    /* 0x02 */ u8 pad2[0x46];
    /* 0x48 */ tProfile **profile;
} PlayerPad; /* 0x4C */

extern PlayerPad gPlayers[];

/* Adds the circuit the circuit player (gCircuitPlayer) finished to its profile's stats for that circuit (the type of
 * its circuit in progress) and to the totals: a circuit, a first, second or third place, the death mode attempts and
 * completions (a place in the top three), the points and the best points, the best time; then writes the stats. A
 * player without a profile uses its "just play" one. */
void RecordCircuit(void) {
    s32 i;
    s32 circuit;
    tCircuitStats *rec;

    if (gPlayers[gCircuitPlayer].profile == 0) {
        gPlayers[gCircuitPlayer].profile = &gJustPlayers[gPlayers[gCircuitPlayer].index];
    }
    circuit = (*(*gPlayers[gCircuitPlayer].profile)->datah)->circuit.type;
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            rec = &(*(*gPlayers[gCircuitPlayer].profile)->datah)->circuitStats[circuit];
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
        if (rec->bestTime == 0.0f || (*(*gPlayers[gCircuitPlayer].profile)->datah)->circuit.totalTime < rec->bestTime) {
            rec->bestTime = (*(*gPlayers[gCircuitPlayer].profile)->datah)->circuit.totalTime;
        }
    }
    SavePlayerCircuitStats(gPlayers[gCircuitPlayer].profile, (*(*gPlayers[gCircuitPlayer].profile)->datah)->circuit.type);
}
