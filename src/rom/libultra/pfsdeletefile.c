/* libultra's osPfsDeleteFile (pfsdeletefile.c): deleting a Controller Pak file. */
#include "../rom_tu.h"

/* Deletes the file with that company, game, name and extension: its pages are freed in every bank and its
 * directory entry cleared. Returns 0 or PFS_ERR_*. */
s32 osPfsDeleteFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name) {
    s32 file_no;
    s32 ret;
    __OSInode inode;
    __OSDir dir;
    __OSInodeUnit last_page;
    u8 startpage;
    u8 bank;

    if (company_code == 0 || game_code == 0) {
        return PFS_ERR_INVALID;
    }

    ERRCK(osPfsFindFile(pfs, company_code, game_code, game_name, ext_name, &file_no));
    SET_ACTIVEBANK_TO_ZERO();
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, pfs->dir_table + file_no, (u8*)&dir));

    startpage = dir.start_page.inode_t.page;

    for (bank = dir.start_page.inode_t.bank; bank < pfs->banks;) {
        ERRCK(__osPfsRWInode(pfs, &inode, PFS_READ, bank));
        ERRCK(__osPfsReleasePages(pfs, &inode, startpage, bank, &last_page));
        ERRCK(__osPfsRWInode(pfs, &inode, PFS_WRITE, bank));

        if (last_page.ipage == PFS_EOF) {
            break;
        }

        bank = last_page.inode_t.bank;
        startpage = last_page.inode_t.page;
    }

    if (bank >= pfs->banks) {
        return PFS_ERR_INCONSISTENT;
    }

    bzero(&dir, sizeof(__OSDir));

    ret = __osContRamWrite(pfs->queue, pfs->channel, pfs->dir_table + file_no, (u8*)&dir, FALSE);

    return ret;
}

/* A page link {bank, page} as its u16: on the N64 that is bank << 8 + page. On the PC, ipage is a host-order
 * u16 and the links are read through the byte fields, so the u16 is the one whose bytes are {bank, page}. */
#define PFS_IPAGE(bank, page) (u16)((page) << 8 | (bank))

/* Frees a file's pages from start_page in bank (the inode table's entries made free). */
s32 __osPfsReleasePages(OSPfs* pfs, __OSInode* inode, u8 start_page, u8 bank, __OSInodeUnit* last_page) {
    __OSInodeUnit next_page;
    __OSInodeUnit old_page;
    s32 ret = 0;

    next_page.ipage = PFS_IPAGE(bank, start_page);

    do {
        old_page = next_page;
        next_page = inode->inode_page[next_page.inode_t.page];
        inode->inode_page[old_page.inode_t.page].ipage = PFS_PAGE_NOT_USED;
    } while (next_page.ipage >= pfs->inode_start_page && next_page.inode_t.bank == bank);

    *last_page = next_page;

    return ret;
}

