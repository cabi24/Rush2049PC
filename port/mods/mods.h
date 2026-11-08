/* port/mods/mods.h: the mods folder (docs/port/MODS.md). PC-layer code only.
 *
 * A mod is a folder (or a .zip) in the mods folder with a mod.ini, files that stand in for the cartridge's (by wave
 * number or by name), and optionally a texture pack (textures/) and text overrides (text/<language>.txt). At start,
 * before the RDRAM window is mapped, main reads the mods that are on (pc_mods_scan), checks each file against the
 * rules the game's loaders need (port/assets/check.c) and against the cartridge's own file, settles which mod's file
 * each wave takes (the first in the order wins), builds the text overrides and works out the memory the larger files
 * need (pc_mods_prepare). Once the game's data is loaded, pc_mods_install puts the files in a second region of the
 * cartridge (pi.c's PC_ROM_MODS_BASE) and points the file table's entries there, so the game reads, converts and sets
 * up each file exactly as it does the cartridge's. Nothing changes without a mod that is on. */
#ifndef PC_MODS_H
#define PC_MODS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_MODS_WAVES 183
#define PC_MODS_MAX 64          /* mods read from the folder */
#define PC_MODS_PATH 1024
#define PC_MODS_WHY 256

/* What the game's file table says of each wave (docs/port/ASSETS.md 2.2), read from the ROM before the game's data is
 * (pc_mods_cart_read), and the ROM itself, for the cartridge's files. */
typedef struct {
    const uint8_t *rom;
    size_t rom_size;
    uint32_t off[PC_MODS_WAVES];    /* wave_rom_offsets: ROM offset */
    uint32_t size[PC_MODS_WAVES];   /* gWaveSizes, gWaveSizes: bytes once read */
    int32_t kind[PC_MODS_WAVES];    /* wave_storage_kinds: 0 raw, 1 LZSS, 2 DEFLATE */
    int text_initial;               /* gLargestTextWave: the text bank read first, whose block the others reuse */
} PcModsCart;

/* One image of port/data's table (PcDataImage's fields that matter here): where the file table's symbols are. */
typedef struct {
    const char *name;
    int deflate;
    uint32_t rom_off, rom_len, size, n64_base;
} PcModsImage;

/* Where the file table's four symbols are: their N64 addresses, which the caller looks up by name in port/data's tables
 * (pc_data_addr_of). gWaveSizes and gLargestTextWave are in the boot image, the other two in the game image. */
typedef struct {
    uint32_t sizes;           /* gWaveSizes: u32[PC_MODS_WAVES] */
    uint32_t text_initial;    /* gLargestTextWave: u8 */
    uint32_t rom_offsets;     /* wave_rom_offsets: u32[PC_MODS_WAVES] */
    uint32_t storage_kinds;   /* wave_storage_kinds: s32[PC_MODS_WAVES] */
} PcModsTableAt;

/* The file table from the ROM's boot and game images, at the addresses `at` gives; 0, or -1 when they don't hold it. */
int pc_mods_cart_read(PcModsCart *c, const uint8_t *rom, size_t rom_size, const PcModsImage *images, int n_images,
                      const PcModsTableAt *at);
/* The cartridge's file of a wave as the game reads it (decompressed): malloc'd, *size bytes; NULL if it can't be. */
uint8_t *pc_mods_cart_file(const PcModsCart *c, int wave, size_t *size);

/* A wave's readable name ("car01", "track1_road") and the folder extract puts it in ("cars"); NULL name for the waves
 * no game code loads (69-75). The name's wave, or -1. */
const char *pc_mods_wave_name(int wave, const char **folder);
int pc_mods_wave_by_name(const char *name);

enum { PC_MOD_FILE_USED, PC_MOD_FILE_REFUSED, PC_MOD_FILE_BEATEN, PC_MOD_FILE_SPARE };

typedef struct PcModFile {
    int wave;
    char path[256];         /* inside the mod: "cars/car01.bin" */
    int stored;             /* the form it was given in: 0 plain, 1 LZSS (.lzs), 2 DEFLATE (.deflate) */
    uint8_t *data;          /* plain, as the game reads it */
    size_t size;
    int state;              /* PC_MOD_FILE_*: used; refused (why); another mod's wins; this mod's other file wins */
    char why[PC_MODS_WHY];
} PcModFile;

/* A text override (text/<language>.txt): string `index` of offset section `section` (0, 2 or 5) of text wave
 * `wave`, as bytes (escapes undone). */
typedef struct {
    int wave, section, index;
    char *bytes;
    size_t len;
} PcModString;

typedef struct PcMod {
    char folder[64];        /* the folder's (or .zip's) name: its key in rush2049pc.ini's [mods] */
    char path[PC_MODS_PATH];
    int zip;
    char name[64], author[64], version[32], description[200], game[32];
    int priority;           /* mod.ini's: higher goes first, unless game.mods_order says otherwise */
    int on;                 /* [mods] folder = on (the default) or off */
    int place;              /* in the order in effect, 0 first; the first mod's file wins */
    char problem[PC_MODS_WHY];  /* the whole mod isn't used: no mod.ini, another game's, ... ("" if fine) */
    PcModFile *files;
    int nfiles;
    int used, refused;      /* its files in use, and refused */
    char textures[PC_MODS_PATH];   /* its texture pack folder, "" for none */
    PcModString *strings;   /* its text overrides */
    int nstrings;
    int texts_used;         /* of them, the strings in use (not another mod's) */
} PcMod;

typedef struct PcMods {
    char dir[PC_MODS_PATH];
    int enabled;            /* game.mods */
    PcMod mod[PC_MODS_MAX];
    int n;
    int order[PC_MODS_MAX]; /* mod indices in the order in effect */
    /* per wave: the winning mod and file (-1: the cartridge's), and the plain bytes the game will read */
    int wave_mod[PC_MODS_WAVES], wave_file[PC_MODS_WAVES];
    uint8_t *wave_data[PC_MODS_WAVES];   /* the file's bytes, or a file made here (text overrides, samples moved) */
    size_t wave_size[PC_MODS_WAVES];
    uint8_t *made[PC_MODS_WAVES];        /* files made here, owned */
    uint64_t growth;        /* the most more heap the files need than the cartridge's, at once */
    int memory;             /* the game.memory pc_mods_prepare settled (0: original) */
    char memory_note[PC_MODS_WHY];
    uint32_t sample_base;   /* the ROM offset the sample directory counts from (pc_mods_prepare's) */
    int text_initial;       /* the text bank the game reads first (the cartridge's gLargestTextWave) */
    uint8_t *region;        /* the mods' cartridge region (pi.c) */
    size_t region_size;
    int installed;          /* waves whose file table entry points into it */
} PcMods;

/* The settings that matter here (from PcSettings: game.mods, mods_dir, mods_order, [mods]). */
typedef struct {
    int enabled;
    const char *dir;        /* the mods folder, resolved */
    const char *order;      /* game.mods_order: folder names, comma-separated, first first */
    int (*is_on)(void *user, const char *folder);   /* [mods] folder: 1 on, 0 off, -1 not listed */
    void *user;
    int memory;             /* game.memory: 0 original, else MiB */
    int expansion_pak;      /* game.expansion_pak */
    int cars;               /* game.cars: 0 original (6) */
    const char *cache;      /* where a .zip mod's texture pack is unpacked ("" for nowhere) */
} PcModsConfig;

/* Reads the mods folder: each mod's mod.ini, files, texture pack and text overrides, in the order in effect. Mods that
 * are off are listed but not read further. 0, or -1 when the folder can't be read (no folder is not an error). */
int pc_mods_scan(PcMods *m, const PcModsConfig *cfg);
/* Checks every file of the mods that are on (port/assets/check.c, with the cartridge's file), settles which file each
 * wave takes, makes the text overrides and the sample directory for moved samples, and the memory the files need:
 * m->memory is cfg->memory, or raised (logged), and a file that needs more than can be had is refused. Logs what it
 * did. sample_base: the ROM offset the sample directory's offsets count from (the game's D_00039B40). */
int pc_mods_prepare(PcMods *m, const PcModsConfig *cfg, const PcModsCart *cart, uint32_t sample_base);
/* Puts the files in the cartridge's mods region (pc_rom_set_mods) and points the file table at them: the game's own
 * tables in the window (offsets wave_rom_offsets, sizes gWaveSizes, kinds wave_storage_kinds). The waves it changed. */
int pc_mods_install(PcMods *m, uint32_t *off, uint32_t *size, int32_t *kind);
/* The texture pack folders of the mods in use, first first, joined by sep: "" for none. */
void pc_mods_texture_dirs(const PcMods *m, char sep, char *out, size_t out_size);
/* One line for the log and the About page: "2 on: Red Cars 1.0, Long Track 0.2" or "none". */
void pc_mods_summary(const PcMods *m, char *out, size_t out_size);
void pc_mods_free(PcMods *m);

/* The decompressors, bounded (a mod's .lzs and .deflate files, the cartridge's files): malloc'd plain bytes, or NULL
 * when the stream is bad or makes more than cap bytes. */
uint8_t *pc_mods_lzss(const uint8_t *in, size_t n, size_t cap, size_t *out_size);
uint8_t *pc_mods_inflate(const uint8_t *in, size_t n, size_t cap, size_t *out_size);

/* A .zip's files (stored or deflated; no zip64, no encryption): each name and its bytes. 0, or -1 with why. */
typedef struct {
    char name[256];
    uint8_t *data;
    size_t size;
} PcZipEntry;
int pc_mods_zip_read(const char *path, PcZipEntry **entries, int *n, char *why, size_t why_size);
void pc_mods_zip_free(PcZipEntry *entries, int n);

#ifdef __cplusplus
}
#endif

#endif
