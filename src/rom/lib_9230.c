/* GENERATED ROM-aligned TU — segment 0x9230 (rom/lib_9230)
 * layout map db0e580c5ebcb31e73af939792e5cb68899d06e5c02bac5a099593008014f6d2.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osPiStartDma (formerly labelled __osPiRawStartDma)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 osPiStartDma(OSIoMesg* mb, s32 priority, s32 direction, u32 devAddr, void* dramAddr, u32 size, OSMesgQueue* mq) {
    register s32 ret;
    if (!__osPiMgrState.flag) {
        return -1;
    }


    if (direction == OS_READ) {
        mb->hdr.type = OS_MESG_TYPE_DMAREAD;
    } else {
        mb->hdr.type = OS_MESG_TYPE_DMAWRITE;
    }

    mb->hdr.pri = priority;
    mb->hdr.retQueue = mq;
    mb->dramAddr = dramAddr;
    mb->devAddr = devAddr;
    mb->size = size;
    mb->piHandle = NULL;

    if (priority == OS_MESG_PRI_HIGH) {
        ret = osJamMesg((OSMesgQueue *)osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
    } else {
        ret = osSendMesg((OSMesgQueue *)osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
    }

    return ret;
}

