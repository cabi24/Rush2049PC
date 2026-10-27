/* port/assets/assets.h: converting the game's files to the host's byte order after they are read from the ROM
 * (docs/port/ASSETS.md §5). PC-layer code only.
 *
 * _ReadWave (src/blob/groups/audio_frame_sync/waves.c) calls pc_wave_loaded once per wave, after the file is in
 * its heap block and before SetupWaveModel relocates it. pc_wave_loaded picks the converter by wave number.
 * Each converter is a strong definition in its own file of this directory. assets.c has a weak no-op for each,
 * so a format that isn't converted yet stays big-endian:
 *
 *   wad.c     WAD container: TOC, OBHD, TXHD, PLHD, PTHD/PATH; registers the big-endian payloads
 *             (TXLD, OBJS display lists, Vtx, IMAG texels and TLUTs) with pc_mem_register_be
 *   world.c   world WADs' WHDR, WOBJ, GDAT, GTLD (after the WAD pass)
 *   road.c    road / collision
 *   path.c    paths
 *   text.c    text bank
 *   font.c    font data
 *   musyx.c   MusyX project, pool, sample directory, songs
 *
 * Strategies (ASSETS.md §5): swap scalar fields in place and keep the N64 size, with pointer fields staying u32
 * window addresses; build a native array beside the file; or leave it big-endian for port/gfx or port/audio,
 * registered with pc_mem_register_be. */
#ifndef PC_ASSETS_H
#define PC_ASSETS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pc_port.h"

/* Wave numbers by format (ASSETS.md §2.3). */
#define PC_WAVE_TEXT_FIRST 0      /* 0..5 text bank, one per language */
#define PC_WAVE_TEXT_LAST 5
#define PC_WAVE_MUSYX_FIRST 6     /* 6..8 project, pool, sample directory; 10..21 songs */
#define PC_WAVE_MUSYX_LAST 21
#define PC_WAVE_FONTWAD_FIRST 22  /* 22..37 font texture WADs */
#define PC_WAVE_FONTWAD_LAST 37
#define PC_WAVE_FONT_FIRST 38     /* 38..53 font data */
#define PC_WAVE_FONT_LAST 53
#define PC_WAVE_WAD_FIRST 54      /* 54..119 model, car and track WADs (101..119 carry PTHD) */
#define PC_WAVE_WAD_LAST 119
#define PC_WAVE_WORLD_FIRST 120   /* 120..138 world WADs */
#define PC_WAVE_WORLD_LAST 138
#define PC_WAVE_ROAD_FIRST 139    /* 139..157 road / collision */
#define PC_WAVE_ROAD_LAST 157
#define PC_WAVE_PATH_FIRST 158    /* 158..182 paths */
#define PC_WAVE_PATH_LAST 182

/* One converter per format. buf is the start of the file in its heap block; size is the block's size, an upper
 * bound on the file's (the file's own headers say how much of it is used). Called once per load: a wave read
 * again into a new block is converted again, from big-endian. */
void pc_assets_wad(int wave, uint8_t *buf, size_t size);
void pc_assets_world(int wave, uint8_t *buf, size_t size);
void pc_assets_road(int wave, uint8_t *buf, size_t size);
void pc_assets_path(int wave, uint8_t *buf, size_t size);
void pc_assets_text(int wave, uint8_t *buf, size_t size);
void pc_assets_font(int wave, uint8_t *buf, size_t size);
void pc_assets_musyx(int wave, uint8_t *buf, size_t size);

/* A wave's heap block goes away (UnloadWaveInstance) or moves (heap compaction): drop what a converter
 * registered for it. The default unregisters the big-endian range that contains buf. */
void pc_assets_wave_released(int wave, uint8_t *buf);

/* Byte order. */
static inline uint16_t pc_be16(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint16_t)(b[0] << 8 | b[1]);
}

static inline uint32_t pc_be32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}

/* In place: big-endian -> native. Native is little-endian on every host the port targets. */
static inline void pc_swap16_at(void *p)
{
    uint16_t v = pc_be16(p);
    memcpy(p, &v, 2);
}

static inline void pc_swap32_at(void *p)
{
    uint32_t v = pc_be32(p);
    memcpy(p, &v, 4);
}

void pc_swap16_n(void *p, size_t count);   /* count consecutive 16-bit fields */
void pc_swap32_n(void *p, size_t count);   /* count consecutive 32-bit fields (f32 too) */

#endif
