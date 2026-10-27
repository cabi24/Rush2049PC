/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Map a physical/cart address into the uncached cartridge address window. */
#ifdef TARGET_PC
/* PC: the MusyX library calls it through its service table as void *(*)(void *), so the word is
 * pointer-sized (docs/port/MUSYX_DATA.md 8.1) */
#define FD60_WORD intptr_t
#else
#define FD60_WORD int
#endif
FD60_WORD func_8010FD60(FD60_WORD arg0) {
    return (arg0 & 0x0FFFFFFF) | 0xB0000000;
}
