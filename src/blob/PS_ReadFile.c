/* PS_ReadFile: read a whole pak file (MAT3 paksysdc.c). */
#include "game/game.h"

/* Reads all of the open file fileh's data from its pak into memory (PS_ReadPartialFile from 0 for its size). */
void PS_ReadFile(void **fileh) {
    PS_ReadPartialFile(fileh, 0, ((PakFileRecord *) *fileh)->size);
}
