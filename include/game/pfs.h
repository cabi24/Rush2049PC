/* include/game/pfs.h: the controller pak's file system records (libultra's os_pfs.h), declared once.
 *
 * The game's files see them through game/types.h, and the boot segment's libultra (src/rom/libultra: osPfsInitPak,
 * osPfsReadWriteFile and the rest of the file system) through PR/os_pfs.h: both include this header, after their
 * basic types. The file system reads and writes a pak's pages through the SI (port/ultra/si.c), 32 bytes at a time:
 * its records hold page numbers and bytes, no address. The pak's pages are a file format (docs/port/SAVES.md); the
 * inode records below are its inode table's entries, whose u16 the PC's pak image keeps in the host's byte order
 * (docs/port/ULTRA.md 3, "Controller pak"). */
#ifndef GAME_PFS_H
#define GAME_PFS_H

struct OSMesgQueue_s;

/* A pak's handle (libultra's OSPfs): what osPfsInitPak read of the pak in a controller, which every other call of the
 * file system takes. The pak system keeps one a controller (tPak.pfs). */
typedef struct OSPfs {
    s32 status;                   /* PFS_INITIALIZED, PFS_ID_BROKEN, PFS_MOTOR_INITIALIZED ... */
    struct OSMesgQueue_s *queue;  /* the queue the SI's completions go to while the file system waits */
    s32 channel;                  /* the controller it is in, 0-3 */
    u8 id[32];                    /* the pak's id block (the serial, the device id, the banks) */
    u8 label[32];                 /* the pak's label */
    s32 version;                  /* the pak's file system version */
    s32 dir_size;                 /* entries in the directory (16) */
    s32 inode_table;              /* the inode table's first block (a page is 8 blocks of 32 bytes) */
    s32 minode_table;             /* its mirror's */
    s32 dir_table;                /* the directory's first block */
    s32 inode_start_page;         /* the first page a file can have */
    u8 banks;                     /* the pak's 32 KiB banks */
    u8 activebank;                /* the bank selected last */
} OSPfs;

/* A file's entry in a pak's directory (libultra's OSPfsState), as osPfsFileState gives it. */
typedef struct OSPfsState {
    u32 file_size;                /* its bytes; 0: no file */
    u32 game_code;
    u16 company_code;
    u8 ext_name[4];               /* in the pak's character set (pak_charset) */
    u8 game_name[16];
} OSPfsState;

/* A page's place in a pak's inode table (libultra's __OSInodeUnit): its bank and page, or as one u16 the next page of a
 * file's chain (PFS_EOF, PFS_PAGE_NOT_USED). */
typedef union __OSInodeUnit {
    struct { u8 bank; u8 page; } inode_t;
    u16 ipage;
} __OSInodeUnit;

/* One bank's inode table (libultra's __OSInode): each of its 128 pages' next page. */
typedef struct __OSInode {
    __OSInodeUnit inode_page[128];
} __OSInode;

#endif /* GAME_PFS_H */
