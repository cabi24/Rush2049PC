/* port/data/overlays.ld: each runtime image's C data in one range (docs/port/DATA.md, section 5).
 *
 * On the N64 a load of runtime image A or B copies the image's .data and zeroes its .bss, so the C of the image
 * starts from its initial values on every load. On the PC that C is linked into the executable. This script gathers
 * the writable data of the objects compiled from src/ovl_a, nonmatching/ovl_a (image A) and src/ovl_b,
 * nonmatching/ovl_b (image B): initialized data, data with relocations, BSS and commons, file-local and
 * function-local statics included. The loader takes each range's bytes at startup and puts them back on every later
 * load of its image (PcDataCRange, pc_data.c). Read-only data stays where it is.
 *
 * Linked with -T: the INSERT makes it an addition to the default script (ELF and PE ld alike). An object belongs to
 * an image by its path, as tools/port/n64syms.py's object_image tells them; `?` takes either kind of slash. */
SECTIONS
{
    .ovl_a : ALIGN(0x1000) {
        pc_ovl_a_start = .;
        *src?ovl_a?*(.data .data.rel .data.rel.local .bss COMMON)
        *nonmatching?ovl_a?*(.data .data.rel .data.rel.local .bss COMMON)
        pc_ovl_a_end = .;
        LONG(0)   /* never empty: PE ld discards an empty section with the symbols in it */
    }
    .ovl_b : ALIGN(0x1000) {
        pc_ovl_b_start = .;
        *src?ovl_b?*(.data .data.rel .data.rel.local .bss COMMON)
        *nonmatching?ovl_b?*(.data .data.rel .data.rel.local .bss COMMON)
        pc_ovl_b_end = .;
        LONG(0)   /* never empty: PE ld discards an empty section with the symbols in it */
    }
}
INSERT AFTER .data;
