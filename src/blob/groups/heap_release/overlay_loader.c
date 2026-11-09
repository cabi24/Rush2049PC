#include "game/game.h"

extern u8 *start_overlay_rom;
extern u8 *extra_overlay_rom;
extern u8 extra_overlay_bss_end [ ];
VIEW(u8[], extra_overlay_bss_start);
#define extra_overlay_bss_start extra_overlay_bss_start_view
extern u8 start_overlay_bss_end [ ];
VIEW(u8[], start_overlay_bss_start);
#define start_overlay_bss_start start_overlay_bss_start_view
extern u8 D_00BE4C70[];
extern u8 D_00BDA100[];

#define N64A(x) PC_N64A(x)
#define N64A0(x) PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))

/* PC (docs/port/DATA.md): both runtime images are linked in, and a load resets the image's data to its ROM
 * values and zeroes its BSS: 0 = image A ("Start", flag start_overlay_loaded), 1 = image B ("Extra", flag
 * extra_overlay_loaded). The game-heap reservation at the image address stays. */
void pc_data_overlay_load(int overlay);
/* No ROM addresses on the PC: the images come from port/data. */
#define D_00BE4C70 0
#define D_00BDA100 0
#define start_overlay_rom 0
#define extra_overlay_rom 0

void load_overlay(u8 *dst, char *name, u8 *end, u8 *bssEnd, u8 *romStart, u8 *bssStart, s8 *loaded, u8 *rom) {
    void *p;

    if (*loaded != 0) {
        return;
    }
    if (&name == 0) {
        return;
    }
    if (dst) {}
    if (bssEnd) {}
    if (bssEnd) {}
    if (bssStart) {}
    p = allocate_memory_at(N64A(dst), end - dst);
    PurgeableMemory(N64A0(p), 0);
    pc_data_overlay_load(loaded == &extra_overlay_loaded);
    *loaded = 1;
}

void load_extra_overlay(void) {
    load_overlay(N64PTR(u8 *, 0x8038A400), "Extra", N64PTR(u8 *, 0x803BB380), extra_overlay_bss_end, D_00BE4C70, extra_overlay_bss_start, &extra_overlay_loaded, extra_overlay_rom);
}

void load_start_overlay(void) {
    load_overlay(N64PTR(u8 *, 0x8038A400), "Start", N64PTR(u8 *, 0x803BB380), start_overlay_bss_end, D_00BDA100, start_overlay_bss_start, &start_overlay_loaded, start_overlay_rom);
}
