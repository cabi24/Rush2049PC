/* MBSetupWad (MAT3 mb_wad.c): WAD files, the containers of the game's model, texture and track data: a table of
 * contents of (tag, address, length) entries (WadEntry, include/game/types.h), rebased in place to where the file
 * was loaded. MBOffsetWad rebases a WAD the heap has moved, MBFindInWad and MBGetFromWad look up a tag. */
#include "game/game.h"

/* A WAD's table rebased in place: its entries' 32-bit N64 addresses are the file's offsets plus the block's N64
 * address, PC_N64A(p) a window pointer's and (T)PC_N64P(a) the pointer at one (pc_prelude.h, docs/port/NOFIXED.md 2.3).
 * Before the rebase the table's address is an offset, which stays as it is. */
#define WAD_ENTRIES(a) ((a) & 0x80000000 ? ((WadEntry *)PC_N64P(a)) : (WadEntry *)(uintptr_t)(a))

/* Fills wad for the WAD file at file. The first time (the table's address an offset, without the window's top bit)
 * the table and every entry are rebased to the file's window address; a WAD already rebased (loaded again from
 * the same block) is only read. MAT3 also swaps the table to its CPU's byte order, which port/assets does on the PC
 * when the file is loaded. */
void MBSetupWad(WadHandle *wad, WadFileHeader *file) {
    s32 i;
    WadEntry *entry;

    if (wad != 0) {
        wad->base = file;
        wad->TOC = WAD_ENTRIES(file->toc);
        wad->TOCentries = file->TOCentries;
        if (!(file->toc & 0x80000000)) {
            wad->TOC = ((WadEntry *)PC_N64P(((u32) wad->TOC + PC_N64A(file))));
            wad->base->toc = PC_N64A(wad->TOC);
            for (i = 0; i < wad->TOCentries; i++) {
                entry = &wad->TOC[i];
                entry->address += PC_N64A(file);
            }
        }
    }
}
