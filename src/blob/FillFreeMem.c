/* FillFreeMem: the heap's marking of freed memory (MAT3 memsys.c's _FillFreeMem). */
#include "game/game.h"

/* The word freed heap memory is filled with, to spot reads of freed blocks (MAT3 fills with bytes 0xEF). */
#define FREED_MEMORY_WORD 0x7FFF0BAD

/* Fills size bytes at ptr (whole words: the last size % 4 bytes are left) with FREED_MEMORY_WORD. */
void FillFreeMem(s32 *ptr, u32 size)
{
    u32 i;

    for (i = 0; i < (size >> 2); i++) {
        ptr[i] = FREED_MEMORY_WORD;
    }
}
