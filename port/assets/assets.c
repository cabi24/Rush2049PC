/* port/assets/assets.c: pc_wave_loaded picks a wave's converter by wave number (assets.h). The weak no-ops below
 * stand in for formats whose converter isn't written yet: those files stay big-endian. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

#define WEAK __attribute__((weak))

WEAK void pc_assets_wad(int wave, uint8_t *buf, size_t size) { (void)wave, (void)buf, (void)size; }
WEAK void pc_assets_world(int wave, uint8_t *buf, size_t size) { (void)wave, (void)buf, (void)size; }
WEAK void pc_assets_road(int wave, uint8_t *buf, size_t size) { (void)wave, (void)buf, (void)size; }
WEAK void pc_assets_path(int wave, uint8_t *buf, size_t size) { (void)wave, (void)buf, (void)size; }
WEAK void pc_assets_text(int wave, uint8_t *buf, size_t size) { (void)wave, (void)buf, (void)size; }
WEAK void pc_assets_font(int wave, uint8_t *buf, size_t size) { (void)wave, (void)buf, (void)size; }
WEAK void pc_assets_musyx(int wave, uint8_t *buf, size_t size) { (void)wave, (void)buf, (void)size; }

static int trace_waves(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RUSH_TRACE_WAVES");
        on = e && *e && *e != '0';
    }
    return on;
}

void pc_wave_loaded(int wave, void *buf, size_t size)
{
    uint8_t *b = (uint8_t *)buf;

    if (trace_waves())
        fprintf(stderr, "rush2049pc: wave %d loaded at %p, block 0x%zx\n", wave, buf, size);
    if (wave >= PC_WAVE_TEXT_FIRST && wave <= PC_WAVE_TEXT_LAST)
        pc_assets_text(wave, b, size);
    else if (wave >= PC_WAVE_MUSYX_FIRST && wave <= PC_WAVE_MUSYX_LAST)
        pc_assets_musyx(wave, b, size);
    else if (wave >= PC_WAVE_FONTWAD_FIRST && wave <= PC_WAVE_FONTWAD_LAST)
        pc_assets_wad(wave, b, size);
    else if (wave >= PC_WAVE_FONT_FIRST && wave <= PC_WAVE_FONT_LAST)
        pc_assets_font(wave, b, size);
    else if (wave >= PC_WAVE_WAD_FIRST && wave <= PC_WAVE_WAD_LAST)
        pc_assets_wad(wave, b, size);
    else if (wave >= PC_WAVE_WORLD_FIRST && wave <= PC_WAVE_WORLD_LAST) {
        pc_assets_wad(wave, b, size);   /* a world WAD is a WAD first */
        pc_assets_world(wave, b, size);
    } else if (wave >= PC_WAVE_ROAD_FIRST && wave <= PC_WAVE_ROAD_LAST)
        pc_assets_road(wave, b, size);
    else if (wave >= PC_WAVE_PATH_FIRST && wave <= PC_WAVE_PATH_LAST)
        pc_assets_path(wave, b, size);
}

void pc_wave_released(int wave, void *buf)
{
    pc_assets_wave_released(wave, (uint8_t *)buf);
}

WEAK void pc_assets_wave_released(int wave, uint8_t *buf)
{
    (void)wave;
    pc_mem_unregister_be(buf);
}

WEAK void pc_assets_wad_block_freed(uint8_t *p, size_t size) { (void)p, (void)size; }

void pc_heap_block_freed(void *p, size_t size)
{
    pc_assets_wad_block_freed((uint8_t *)p, size);
    pc_mem_unregister_be(p);   /* a file the PI DMA or a decompressor registered at the block's start */
}

void pc_swap16_n(void *p, size_t count)
{
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < count; i++)
        pc_swap16_at(b + 2 * i);
}

void pc_swap32_n(void *p, size_t count)
{
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < count; i++)
        pc_swap32_at(b + 4 * i);
}
