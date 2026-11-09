/* libultra's serial bus access lock (siacs.c) and a controller's pak status (pfsgetstatus.c). */
#include "../rom_tu.h"

/* Makes the serial bus's lock, a one-message queue (once). */
void __osSiCreateAccessQueue(void) {
    __osSiInitialized = 1;
    osCreateMesgQueue(&__osSiMesg, &__osSiMesgQueue, 1);
    osSendMesg(&__osSiMesg, NULL, 0);
}

/* Takes the serial bus's lock (making it first if need be). */
void __osSiGetAccess(void) {
    OSMesg dummyMesg;
    if (!__osSiInitialized) {
        __osSiCreateAccessQueue();
    }
    osRecvMesg(&__osSiMesg, &dummyMesg, OS_MESG_BLOCK);
}

/* Gives the serial bus's lock back. */
void __osSiRelAccess(void) {
    osSendMesg(&__osSiMesg, NULL, OS_MESG_NOBLOCK);
}

/* The pak status of controller channel. Returns 0, PFS_ERR_NOPACK, PFS_ERR_NEW_PACK or PFS_ERR_CONTRFAIL. */
s32 __osPfsGetStatus(OSMesgQueue* queue, int channel) {
    s32 ret = 0;
    OSMesg dummy;
    OSContStatus data;

    __osSiChannelMask = 250;

    __osPfsRequestOneChannel(channel, CONT_CMD_REQUEST_STATUS);

    ret = __osSiRawStartDma(OS_WRITE, &(*(OSPifRam *)__osPfsPifRam));
    osRecvMesg(queue, &dummy, OS_MESG_BLOCK);

    ret = __osSiRawStartDma(OS_READ, &(*(OSPifRam *)__osPfsPifRam));
    osRecvMesg(queue, &dummy, OS_MESG_BLOCK);

    __osPfsGetOneChannelData(channel, &data);

    if (((data.status & CONT_CARD_ON) != 0) && ((data.status & CONT_CARD_PULL) != 0)) {
        return PFS_ERR_NEW_PACK;
    } else if ((data.errno != 0) || ((data.status & CONT_CARD_ON) == 0)) {
        return PFS_ERR_NOPACK;
    } else if ((data.status & CONT_ADDR_CRC_ER) != 0) {
        return PFS_ERR_CONTRFAIL;
    }

    return ret;
}

/* Fills the PIF buffer with command cmd for controller channel only. */
void __osPfsRequestOneChannel(int channel, u8 cmd) {
    u8* ptr;
    __OSContRequesFormatShort requestformat;
    int i;

    __osContLastCmd = CONT_CMD_END;
    (*(OSPifRam *)__osPfsPifRam).pifstatus = CONT_CMD_READ_BUTTON;

    ptr = (u8*)&(*(OSPifRam *)__osPfsPifRam);

    requestformat.txsize = CONT_CMD_REQUEST_STATUS_TX;
    requestformat.rxsize = CONT_CMD_REQUEST_STATUS_RX;
    requestformat.cmd = cmd;
    requestformat.typeh = CONT_CMD_NOP;
    requestformat.typel = CONT_CMD_NOP;
    requestformat.status = CONT_CMD_NOP;

    for (i = 0; i < channel; i++) {
        *ptr++ = CONT_CMD_REQUEST_STATUS;
    }

    *(__OSContRequesFormatShort*)ptr = requestformat;
    ptr += sizeof(__OSContRequesFormatShort);
    *ptr = CONT_CMD_END;
}

/* Reads the status of controller channel from the PIF buffer into data. */
void __osPfsGetOneChannelData(int channel, OSContStatus* data) {
    u8* ptr = (u8*)&(*(OSPifRam *)__osPfsPifRam);
    __OSContRequesFormatShort requestformat;
    int i;

    for (i = 0; i < channel; i++) {
        ptr++;
    }

    requestformat = *(__OSContRequesFormatShort*)ptr;
    data->errno = CHNL_ERR(requestformat);

    if (data->errno != 0) {
        return;
    }

    data->type = (requestformat.typel << 8) | (requestformat.typeh);
    data->status = requestformat.status;
}

