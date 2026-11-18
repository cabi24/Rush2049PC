/* SavePlayerCircuitStats and SavePlayerCircuit: write a profile's circuit records (MAT3 save.c). */
#include "game/game.h"

/* Gives the profile's stats of circuit kind their checksum again and marks them to write (the caller changed them).
 * The file must be read. */
void SavePlayerCircuitStats(tProfile **profile,u8 kind)
{
    tCircuitStats *record=&(*(*profile)->datah)->circuitStats[kind];
    record->checksum=CalcChecksumInternal(&record->numCircuits,sizeof(tCircuitStats)-4);
    PS_WriteFile((*profile)->fileh,&record->checksum,sizeof(tCircuitStats));
}

/* The same for the profile's circuit in progress. */
void SavePlayerCircuit(tProfile **profile)
{
    tCircuit *record=&(*(*profile)->datah)->circuit;
    record->checksum=CalcChecksumInternal(&record->active,sizeof(tCircuit)-4);
    PS_WriteFile((*profile)->fileh,record,sizeof(tCircuit));
}
