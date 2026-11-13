/* load_overlay, load_extra_overlay and load_start_overlay: load a runtime image (the N64's overlay loader; on the
 * PC both images are linked in and a load resets one's data, docs/port/DATA.md). */
#include "game/game.h"

/* PC (docs/port/DATA.md): both runtime images are linked in, and a load resets the image's data to its ROM
 * values and zeroes its BSS: 0 = image A ("Start", flag start_overlay_loaded), 1 = image B ("Extra", flag
 * extra_overlay_loaded). The heap still reserves the image's block, so what fits beside it is the N64's. */
void pc_data_overlay_load(int overlay);

/* The heap block the runtime images load into: PC_RUNTIME_IMAGE_SIZE bytes, PC_RUNTIME_IMAGE_OFFSET bytes past the
 * main heap's header, the place the N64 loaded them at (pc_game_types.h has its numbers). Both images share it:
 * loading one frees the other first (LoadFrontEndOverlay, unload_extra_overlay). */
u8 *runtime_image_block(void) {
    return (u8 *)gMainHeap + PC_RUNTIME_IMAGE_OFFSET;
}

/* Loads a runtime image unless it is (*loaded): the heap block from dst to end is taken for it (allocate_memory_at)
 * and made purgeable, the image's data reset (pc_data_overlay_load: which image, from loaded), and *loaded set. The
 * N64's loader also took the image's name, its place in the ROM and its BSS's bounds, to inflate it and clear the
 * BSS; on the PC the data loader does both from its tables. */
void load_overlay(u8 *dst, u8 *end, s8 *loaded) {
    void *p;

    if (*loaded != 0) {
        return;
    }
    p = allocate_memory_at(dst, end - dst);
    PurgeableMemory(p, 0);
    pc_data_overlay_load(loaded == &extra_overlay_loaded);
    *loaded = 1;
}

/* Loads runtime image B, the battle and stunt HUD extras. */
void load_extra_overlay(void) {
    u8 *block = runtime_image_block();

    load_overlay(block, block + PC_RUNTIME_IMAGE_SIZE, &extra_overlay_loaded);
}

/* Loads runtime image A, the front end. */
void load_start_overlay(void) {
    u8 *block = runtime_image_block();

    load_overlay(block, block + PC_RUNTIME_IMAGE_SIZE, &start_overlay_loaded);
}
