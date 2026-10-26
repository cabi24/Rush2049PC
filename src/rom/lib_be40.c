/* GENERATED ROM-aligned TU — segment 0xbe40 (rom/lib_be40)
 * layout map ccd8220c25986fe69d3e9a92b2007a65f55e0d15beb31f6dd2ecb298eb49485e.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osPfsFreeBlocks
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes_not_used) {
    int j;
    int pages = 0;
    __OSInode inode;
    s32 ret = 0;
    u8 bank;
    int offset;

    PFS_CHECK_STATUS();
    ERRCK(__osCheckId(pfs));
    for (bank = 0; bank < pfs->banks; bank++) {
        ERRCK(__osPfsRWInode(pfs, &inode, PFS_READ, bank));
        offset = ((bank > 0) ? 1 : pfs->inode_start_page);

        for (j = offset; j < ARRLEN(inode.inode_page); j++) {
            if (inode.inode_page[j].ipage == PFS_PAGE_NOT_USED) {
                pages++;
            }
        }
    }

    *bytes_not_used = pages * PFS_ONE_PAGE * BLOCKSIZE;
    return 0;
}

