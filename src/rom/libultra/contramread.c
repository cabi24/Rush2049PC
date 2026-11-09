/* libultra's __osContRamRead: reading a block of a Controller Pak's memory. */
#include "../rom_tu.h"

/* Reads the 32-byte block at address (a block number) of the pak on controller channel into buffer, through the
 * PIF; the data's CRC is checked and the read tried twice more. Returns 0, or PFS_ERR_* (no pak, a new pak,
 * CONTRP_CRC_ERROR). */
s32 __osContRamRead(OSMesgQueue* mq, int channel, u16 address, u8* buffer) {
    s32 ret = 0;
    s32 i;
    u8* ptr;
    s32 retry = 2;

    __osSiGetAccess();

    do {
        ptr = (u8*)&(*(OSPifRam *)__osPfsPifRam);

        if (__osContLastCmd != CONT_CMD_READ_PAK || (u32)__osContLastChannel != channel) {
            __osContLastCmd = CONT_CMD_READ_PAK;
            __osContLastChannel = channel;

            for (i = 0; i < channel; i++) { *ptr++ = CONT_CMD_REQUEST_STATUS; }

            (*(OSPifRam *)__osPfsPifRam).pifstatus = CONT_CMD_EXE;

            READFORMAT(ptr)->dummy = CONT_CMD_NOP;
            READFORMAT(ptr)->txsize = CONT_CMD_READ_PAK_TX;
            READFORMAT(ptr)->rxsize = CONT_CMD_READ_PAK_RX;
            READFORMAT(ptr)->cmd = CONT_CMD_READ_PAK;
            READFORMAT(ptr)->datacrc = 0xFF;

            ptr[sizeof(__OSContRamReadFormat)] = CONT_CMD_END;
        } else {
            ptr += channel;
        }

        READFORMAT(ptr)->addrh = address >> 3;
        READFORMAT(ptr)->addrl = (u8)((address << 5) | __osContAddressCrc(address));


        ret = __osSiRawStartDma(OS_WRITE, &(*(OSPifRam *)__osPfsPifRam));
        osRecvMesg(mq, NULL, OS_MESG_BLOCK);

        ret = __osSiRawStartDma(OS_READ, &(*(OSPifRam *)__osPfsPifRam));
        osRecvMesg(mq, NULL, OS_MESG_BLOCK);

        ret = CHNL_ERR(*READFORMAT(ptr));

        if (!ret) {
            if (__osPfsDataChecksum(READFORMAT(ptr)->data) != READFORMAT(ptr)->datacrc) {
                ret = __osPfsGetStatus(mq, channel);

                if (ret) {
                    break;
                } else {
                    ret = PFS_ERR_CONTRFAIL;
                }
            } else {
                bcopy(READFORMAT(ptr)->data, buffer, BLOCKSIZE);
            }
        } else {
            ret = PFS_ERR_NOPACK;
        }
    } while ((ret == PFS_ERR_CONTRFAIL) && (retry-- >= 0));
    __osSiRelAccess();
    return ret;
}

