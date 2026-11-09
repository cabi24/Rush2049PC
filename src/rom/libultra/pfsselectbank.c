/* libultra's __osPfsSelectBank (pfsselectbank.c): choosing a Controller Pak's memory bank. */
#include "../rom_tu.h"

/* Switches the pak to memory bank bank (a write to its bank register). Returns 0 or PFS_ERR_*. */
s32 __osPfsSelectBank(OSPfs* pfs, u8 bank) {
    u8 temp[BLOCKSIZE];
    int i;
    s32 ret = 0;

    for (i = 0; i < BLOCKSIZE; i++) {
        temp[i] = bank;
    }

    ret = __osContRamWrite(pfs->queue, pfs->channel, CONT_BLOCK_DETECT, temp, FALSE);

    if (ret == 0) {
        pfs->activebank = bank;
    }

    return ret;
}

