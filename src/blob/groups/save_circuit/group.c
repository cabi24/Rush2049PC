/* SavePlayerCircuitStats and SavePlayerCircuit: write a profile's circuit records (MAT3 save.c). */
#include "game/game.h"

extern void PS_WriteFile(void *,u32 *,int);

/* Gives the profile's stats of circuit kind their checksum again and marks them to write (the caller changed them).
 * The file must be read. */
void SavePlayerCircuitStats(tProfile **profile,u8 kind)
{
    tCircuitStats *record=&(*(*profile)->datah)->circuitStats[kind];
    record->checksum=CalcChecksumInternal((u8 *)record+4,24);
    PS_WriteFile((*profile)->fileh,&record->checksum,28);
}

/* The same for the profile's circuit in progress. */
void SavePlayerCircuit(tProfile **profile)
{
    u8 *record=(u8 *)&(*(*profile)->datah)->circuit;
    *(u32 *)record=CalcChecksumInternal(record+4,72);
    PS_WriteFile((*profile)->fileh,(u32 *)record,76);
}
