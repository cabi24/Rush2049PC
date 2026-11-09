/* load_overlay, load_extra_overlay and load_start_overlay: load a runtime image (the N64's overlay loader; on the
 * PC both images are linked in and a load resets one's data, docs/port/DATA.md). */
#include "game/game.h"

extern u8 extra_overlay_bss_end [ ];
VIEW(u8[], extra_overlay_bss_start);
#define extra_overlay_bss_start extra_overlay_bss_start_view
extern u8 start_overlay_bss_end [ ];

/* The game heap's region the runtime images are loaded into (the N64's addresses, which the heap still keeps; the
 * memory model's wave 3 makes it PC memory). */
#define RUNTIME_IMAGE_START 0x8038A400
#define RUNTIME_IMAGE_END   0x803BB380

/* PC (docs/port/DATA.md): both runtime images are linked in, and a load resets the image's data to its ROM
 * values and zeroes its BSS: 0 = image A ("Start", flag start_overlay_loaded), 1 = image B ("Extra", flag
 * extra_overlay_loaded). The game-heap reservation at the image address stays. */
void pc_data_overlay_load(int overlay);

/* Loads a runtime image unless it is (*loaded): the heap block from dst to end is taken for it (allocate_memory_at)
 * and made purgeable, the image's data reset (pc_data_overlay_load: which image, from loaded), and *loaded set. On
 * the PC name, bssEnd, romStart, bssStart and rom are unused (the N64 inflated the image from rom there). */
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
    p = allocate_memory_at(PC_N64A(dst), end - dst);
    PurgeableMemory(PC_N64A(p), 0);
    pc_data_overlay_load(loaded == &extra_overlay_loaded);
    *loaded = 1;
}

/* Loads runtime image B, the battle and stunt HUD extras. */
void load_extra_overlay(void) {
    load_overlay(((u8 *)PC_N64P(RUNTIME_IMAGE_START)), "Extra", ((u8 *)PC_N64P(RUNTIME_IMAGE_END)), extra_overlay_bss_end, 0, extra_overlay_bss_start, &extra_overlay_loaded, 0);
}

/* Loads runtime image A, the front end. */
void load_start_overlay(void) {
    load_overlay(((u8 *)PC_N64P(RUNTIME_IMAGE_START)), "Start", ((u8 *)PC_N64P(RUNTIME_IMAGE_END)), start_overlay_bss_end, 0, start_overlay_bss_start, &start_overlay_loaded, 0);
}
