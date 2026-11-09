/* libultra's controller status and read (contquery.c, contreaddata.c): osContStartQuery, osContGetQuery,
 * osContStartReadData, osContGetReadData. */
#include "../rom_tu.h"

/* Asks every controller its status (into the PIF), under the serial bus's lock. Returns the serial bus's result. */
s32 osContStartQuery(OSMesgQueue* mq) {
    s32 ret = 0;

    __osSiGetAccess();

    if (__osContLastCmd != CONT_CMD_REQUEST_STATUS) {
        __osPackRequestData(CONT_CMD_REQUEST_STATUS);
        ret = __osSiRawStartDma(OS_WRITE, __osSiDmaBuffer.ramarray);
        osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    }

    ret = __osSiRawStartDma(OS_READ, __osSiDmaBuffer.ramarray);
    __osContLastCmd = CONT_CMD_REQUEST_STATUS;
    __osSiRelAccess();
    return ret;
}

/* Each controller's status, from the last query, into data. */
void osContGetQuery(OSContStatus* data) {
    u8 pattern;
    __osContGetInitData(&pattern, data);
}

/* Asks every controller its buttons and stick (into the PIF), under the serial bus's lock. Returns the serial bus's
 * result. */
s32 osContStartReadData(OSMesgQueue *mq) {
    s32 ret;
    s32 unused;

    __osSiGetAccess();
    if (__osContLastCmd != CONT_CMD_READ_BUTTON) {
        __osPackReadData();
        __osSiRawStartDma(OS_WRITE, &__osSiDmaBuffer);
        osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    }
    ret = __osSiRawStartDma(OS_READ, &__osSiDmaBuffer);
    unused = ret;
    __osContLastCmd = CONT_CMD_READ_BUTTON;
    __osSiRelAccess();
    return ret;
}

/* Each controller's buttons and stick, from the last read, into data (errno set where none answered). */
void osContGetReadData(OSContPad* data) {
    u8* ptr = (u8*)__osSiDmaBuffer.ramarray;
    __OSContReadFormat readformat;
    int i;

    for (i = 0; i < __osMaxControllers; i++, ptr += sizeof(__OSContReadFormat), data++) {
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

/* Fills the PIF buffer with the read command for every controller. */
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

    for (i = 0; i < __osMaxControllers; i++) {
        *(__OSContReadFormat*)ptr = readformat;
        ptr += sizeof(__OSContReadFormat);
    }

    *ptr = CONT_CMD_END;
}

