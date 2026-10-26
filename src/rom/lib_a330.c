/* GENERATED ROM-aligned TU — segment 0xa330 (rom/lib_a330)
 * layout map a35c7b19bb677a9029630ff9414a70c9be081a07d21c4a5bb277be656d9b0153.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osContStartQuery
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 osContStartQuery(OSMesgQueue* mq) {
    s32 ret = 0;

    __osSiGetAccess();

    if (__osPfsRequestType != CONT_CMD_REQUEST_STATUS) {
        __osPackRequestData(CONT_CMD_REQUEST_STATUS);
        ret = __osSiRawStartDma(OS_WRITE, __osSiDmaBuffer.ramarray);
        osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    }

    ret = __osSiRawStartDma(OS_READ, __osSiDmaBuffer.ramarray);
    __osPfsRequestType = CONT_CMD_REQUEST_STATUS;
    __osSiRelAccess();
    return ret;
}

/* PROMOTED 2026-10-01 — osContGetQuery
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osContGetQuery(OSContStatus* data) {
    u8 pattern;
    __osContGetInitData(&pattern, data);
}

/* PROMOTED 2026-10-01 — osContStartReadData (formerly labelled osContStartReadData2)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 osContStartReadData(OSMesgQueue *arg0) {
    s32 temp_v0;
    s32 sp1C;

    __osSiGetAccess();
    if (__osPfsRequestType != 1) {
        __osPackReadData();
        __osSiRawStartDma(1, &__osSiDmaBuffer);
        osRecvMesg(arg0, NULL, 1);
    }
    temp_v0 = __osSiRawStartDma(0, &__osSiDmaBuffer);
    sp1C = temp_v0;
    __osPfsRequestType = 1;
    __osSiRelAccess();
    return temp_v0;
}

/* PROMOTED 2026-10-01 — osContGetReadData
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osContGetReadData(OSContPad* data) {
    u8* ptr = (u8*)__osSiDmaBuffer.ramarray;
    __OSContReadFormat readformat;
    int i;

    for (i = 0; i < __osPfsRequestType2; i++, ptr += sizeof(__OSContReadFormat), data++) {
        readformat = *(__OSContReadFormat*)ptr;
        data->errno = CHNL_ERR(readformat);

        if (data->errno != 0) {
            continue;
        }

        data->button = readformat.button;
        data->stick_x = readformat.stick_x;
        data->stick_y = readformat.stick_y;
    }
}

/* PROMOTED 2026-10-01 — __osPackReadData
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __osPackReadData(void) {
    u8* ptr = (u8*)__osSiDmaBuffer.ramarray;
    __OSContReadFormat readformat;
    int i;

    for (i = 0; i < ARRLEN(__osSiDmaBuffer.ramarray); i++) {
        __osSiDmaBuffer.ramarray[i] = 0;
    }

    __osSiDmaBuffer.pifstatus = CONT_CMD_EXE;
    readformat.dummy = CONT_CMD_NOP;
    readformat.txsize = CONT_CMD_READ_BUTTON_TX;
    readformat.rxsize = CONT_CMD_READ_BUTTON_RX;
    readformat.cmd = CONT_CMD_READ_BUTTON;
    readformat.button = 0xFFFF;
    readformat.stick_x = -1;
    readformat.stick_y = -1;

    for (i = 0; i < __osPfsRequestType2; i++) {
        *(__OSContReadFormat*)ptr = readformat;
        ptr += sizeof(__OSContReadFormat);
    }

    *ptr = CONT_CMD_END;
}

