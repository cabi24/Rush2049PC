/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * pak_poll_inserted (historical label) -- track Controller Pak insertion.
 * When gPakSysInited is set, for each of the four ports read the SDK controller
 * status (cont_status is OSContStatus[4]: u16 type, u8 status, u8 errno; status
 * bit 0 = CONT_CARD_ON, bit 1 = CONT_CARD_PULL): a pak that is on and not pulled
 * marks the port's pak record (gPaks[port], 772 bytes) inserted (+2); a port
 * that was marked and no longer has a pak is cleared and flagged changed (+3).
 * N64 only, no arcade ancestor.
 *
 * Shaping: both the status array and the pak table are volatile (retail reloads
 * the status byte for each bit test and keeps every pak byte access).  With the
 * volatile accesses IDO unrolls the natural four-port loop by two, which is
 * retail's "pair" loop (the earlier non-volatile attempts never unrolled).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int s32;

typedef struct OSContStatus { u16 type; u8 status; u8 errno_; } OSContStatus;
/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct Pak772 { u8 index; s8 enabled; s8 inserted; s8 changed; u8 opaque4[8]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } Pak772;

extern s8 gPakSysInited;
extern volatile Pak772 gPaks[4];
extern volatile OSContStatus cont_status[4];

void pak_poll_inserted(void)
{
    s32 i;

    if (gPakSysInited) {
        for (i = 0; i < 4; i++) {
            if ((cont_status[i].status & 1) && !(cont_status[i].status & 2)) {
                if (!gPaks[i].inserted) {
                    gPaks[i].inserted = 1;
                }
            } else if (gPaks[i].inserted) {
                gPaks[i].inserted = 0;
                gPaks[i].changed = 1;
            }
        }
    }
}
