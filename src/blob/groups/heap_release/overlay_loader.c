/* load_overlay, load_extra_overlay and load_start_overlay: load a runtime image (the N64's overlay loader; on the
 * PC both images are linked in and a load resets one's data, docs/port/DATA.md). */
#include "game/game.h"

extern u8 extra_overlay_bss_end [ ];
VIEW(u8[], extra_overlay_bss_start);
#define extra_overlay_bss_start extra_overlay_bss_start_view
extern u8 start_overlay_bss_end [ ];

/* PC (docs/port/DATA.md): both runtime images are linked in, and a load resets the image's data to its ROM
 * values and zeroes its BSS: 0 = image A ("Start", flag start_overlay_loaded), 1 = image B ("Extra", flag
 * extra_overlay_loaded). The heap still reserves the image's block, so what fits beside it is the N64's. */
void pc_data_overlay_load(int overlay);

/* The heap block the runtime images load into: PC_RUNTIME_IMAGE_SIZE bytes, PC_RUNTIME_IMAGE_OFFSET bytes past the main
 * heap's header (pc_game_types.h), where the N64 loaded them (0x8038A400 to 0x803BB380). Both images share it:
 * loading one frees the other first (LoadFrontEndOverlay, unload_extra_overlay). */
u8 *runtime_image_block(void) {
    return (u8 *)gMainHeap + PC_RUNTIME_IMAGE_OFFSET;
}

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
    p = allocate_memory_at(dst, end - dst);
    PurgeableMemory(p, 0);
    pc_data_overlay_load(loaded == &extra_overlay_loaded);
    *loaded = 1;
}

/* Loads runtime image B, the battle and stunt HUD extras. */
void load_extra_overlay(void) {
    u8 *block = runtime_image_block();

    load_overlay(block, "Extra", block + PC_RUNTIME_IMAGE_SIZE, extra_overlay_bss_end, 0, extra_overlay_bss_start, &extra_overlay_loaded, 0);
}

/* Loads runtime image A, the front end. */
void load_start_overlay(void) {
    u8 *block = runtime_image_block();

    load_overlay(block, "Start", block + PC_RUNTIME_IMAGE_SIZE, start_overlay_bss_end, 0, start_overlay_bss_start, &start_overlay_loaded, 0);
}
