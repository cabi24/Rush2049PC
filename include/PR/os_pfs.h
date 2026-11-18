/**
 * @file os_pfs.h
 * @brief N64 Controller Pak (PFS) filesystem definitions
 *
 * Based on Nintendo SDK headers and reference decomp projects.
 */

#ifndef _OS_PFS_H_
#define _OS_PFS_H_

#include "types.h"
#include "PR/os_message.h"
#include "PR/os_thread.h"

/* The pak's handle (OSPfs), a file's directory entry (OSPfsState) and the inode table's records (__OSInodeUnit,
 * __OSInode): declared once, for this libultra and the game's files alike. */
#include "game/pfs.h"

/**
 * PFS Directory Entry
 *
 * Single directory entry on the Controller Pak.
 */
typedef struct __OSDir {
    u32 game_code;
    u16 company_code;
    __OSInodeUnit start_page;
    u8 status;
    s8 reserved;
    u16 data_sum;
    u8 ext_name[4];
    u8 game_name[16];
} __OSDir;

/* File System Constants */
#define OS_PFS_VERSION      0x0200
#define OS_PFS_VERSION_HI   (OS_PFS_VERSION >> 8)
#define OS_PFS_VERSION_LO   (OS_PFS_VERSION & 255)

#define PFS_FILE_NAME_LEN   16
#define PFS_FILE_EXT_LEN    4
#define PFS_BLOCKSIZE       32      /* bytes per block */
#define PFS_ONE_PAGE        8       /* blocks per page */
#define PFS_PAGE_SIZE       32      /* bytes per page (same as block) */
#define PFS_MAX_BANKS       62

/* Inode special values */
#define PFS_EOF             1       /* End of file chain */
#define PFS_PAGE_FREE       3       /* Free page marker */

/* Controller Pak Commands */
#define PFS_CMD_READ        2       /* Read page command */
#define PFS_CMD_WRITE       3       /* Write page command */

/* File System Flags */
#define PFS_READ            0
#define PFS_WRITE           1
#define PFS_CREATE          2

/* File System Status Flags */
#define PFS_INITIALIZED         0x01
#define PFS_CORRUPTED           0x02
#define PFS_ID_BROKEN           0x04
#define PFS_MOTOR_INITIALIZED   0x08
#define PFS_GBPAK_INITIALIZED   0x10

/* File System Error Codes */
#define PFS_ERR_SUCCESS         0   /* No error */
#define PFS_ERR_NOPACK          1   /* No memory card plugged in */
#define PFS_ERR_NEW_PACK        2   /* Memory card changed */
#define PFS_ERR_INCONSISTENT    3   /* Need to run checker */
#define PFS_ERR_CONTRFAIL       4   /* Controller overrun error */
#define PFS_ERR_INVALID         5   /* Invalid parameter or file not found */
#define PFS_ERR_BAD_DATA        6   /* Bad data read from pack */
#define PFS_ERR_FULL            7   /* No free pages */
#define PFS_ERR_DIR_FULL        8   /* No free directory entries */
#define PFS_ERR_EXIST           9   /* File already exists */
#define PFS_ERR_ID_FATAL        10  /* Dead memory card */
#define PFS_ERR_DEVICE          11  /* Wrong device type */

/* Function Prototypes */
s32 osPfsInitPak(OSMesgQueue *queue, OSPfs *pfs, s32 channel);
s32 osPfsChecker(OSPfs *pfs);
s32 osPfsRepairId(OSPfs *pfs);
s32 osPfsAllocateFile(OSPfs *pfs, u16 companyCode, u32 gameCode,
                      u8 *gameName, u8 *extName, s32 size, s32 *fileNo);
s32 osPfsFileState(OSPfs *pfs, s32 fileNo, OSPfsState *state);
s32 osPfsGetLabel(OSPfs *pfs, u8 *label, s32 *length);
s32 osPfsSetLabel(OSPfs *pfs, u8 *label);
s32 osPfsFreeBlocks(OSPfs *pfs, s32 *freeBytes);
s32 osPfsNumFiles(OSPfs *pfs, s32 *maxFiles, s32 *usedFiles);

/* Internal PFS Functions */
s32 __osPfsSelectBank(OSPfs *pfs, u8 bank);
s32 __osPfsRWInode(OSPfs *pfs, __OSInode *inode, u8 flag, u8 bank);
s32 osPfsFindFile(OSPfs *pfs, u16 companyCode, u32 gameCode,
                  u8 *gameName, u8 *extName, s32 *fileNo);
/* VERSION_J __osPfsReleasePages (io/pfsdeletefile.c). */
s32 __osPfsReleasePages(OSPfs *pfs, __OSInode *inode, u8 start_page, u8 bank,
                        __OSInodeUnit *last_page);

#endif /* _OS_PFS_H_ */
