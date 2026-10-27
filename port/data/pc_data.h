/* port/data/pc_data.h: the descriptors that tell the loader (pc_data.c) how to fill the game's data from the
 * ROM (docs/port/DATA.md). tools/port/gen_data.py writes the game's tables (build/pc/pc-data/data.c); the unit
 * test (test_pc_data.c) writes its own. PC-layer code only.
 *
 * The tables hold no ROM bytes: only N64 addresses, sizes and layouts. Every value comes from the ROM at run
 * time, so an overlay "load" is the same fill run again. */
#ifndef PC_DATA_H
#define PC_DATA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Element kinds. N64 size / host size: U8 1/1, U16 2/2, U32 and F32 4/4, U64 and F64 8/8, PTR 4/pointer,
 * S32_64 and U32_64 4/8 (the N64's 32-bit long). Values are big-endian on the N64. */
enum {
    PC_K_U8 = 1,
    PC_K_U16,
    PC_K_U32,
    PC_K_F32,
    PC_K_U64,
    PC_K_F64,
    PC_K_PTR,
    PC_K_S32_64,
    PC_K_U32_64,
    PC_K_COUNT
};

/* A run of `count` contiguous elements of one kind, repeated `rep` times with the given strides. Offsets are
 * from the start of the symbol (own storage) or of the arena (arena runs, where host = N64 offsets). */
typedef struct {
    uint32_t n64_off, host_off;
    uint32_t count, rep;
    uint32_t n64_stride, host_stride;
    uint8_t kind;
} PcDataRun;

/* Where an image's bytes are in the ROM. */
enum { PC_IMG_RAW = 0, PC_IMG_DEFLATE = 1 };
typedef struct {
    const char *name;
    uint8_t storage;            /* PC_IMG_RAW or PC_IMG_DEFLATE */
    uint32_t rom_off, rom_len;  /* the bytes or the raw DEFLATE stream */
    uint32_t size;              /* image size */
    uint32_t n64_base;          /* N64 address of the image's first byte */
} PcDataImage;

/* One image's data and BSS in N64 layout: [n64_lo, n64_init) is initialized from the image, [n64_init,
 * n64_hi) is zeroed. The runs byte-swap the known elements in place. The game's arenas are at their N64
 * addresses (host == n64_lo, in the RDRAM window); a runtime image's arena there is cleared again after the first
 * load, since it is game-heap memory until the game loads the image. */
typedef struct {
    uint8_t image;
    uint32_t n64_lo, n64_init, n64_hi;
    uint8_t *host;
    uint32_t run_first, run_count;
} PcDataArena;

/* A symbol with its own host storage (PC_SYM_OWN: filled from the image through its runs, or zeroed if
 * PC_SYM_BSS), or one the game C defines (PC_SYM_CDEF: listed so pointers to it resolve, and, in an overlay,
 * restored on load: PC_SYM_BSS zeroes it, otherwise its startup bytes are put back). Sorted by n64_addr. */
enum { PC_SYM_OWN = 1, PC_SYM_CDEF = 2, PC_SYM_BSS = 4 };
typedef struct {
    const char *name;
    uint32_t n64_addr, n64_size;
    uint32_t host_size;
    void *host;                 /* NULL when a PC_SYM_CDEF symbol is not linked in */
    uint32_t run_first, run_count;
    uint8_t image, flags;
} PcDataSym;

/* A function the game's data may point to. host is NULL when the function has no C yet. Sorted by n64_addr;
 * the two runtime images can each have a function at the same address. */
typedef struct {
    uint32_t n64_addr;
    uint8_t image;
    void *host;
    const char *name;
} PcDataFunc;

/* Arena symbols by address, for reports. */
typedef struct {
    uint32_t n64_addr;
    const char *name;
} PcDataName;

/* Mutable loader state (one per table set). */
typedef struct {
    const uint8_t *rom;
    size_t rom_size;
    uint8_t *image[8];          /* inflated images (raw images point into the ROM) */
    uint8_t *owned[8];          /* buffers to free */
    void **snapshot;            /* per symbol: startup bytes of PC_SYM_CDEF overlay data */
    int loaded;
} PcDataState;

typedef struct {
    const PcDataImage *images;
    uint32_t n_images;
    const PcDataArena *arenas;
    uint32_t n_arenas;
    const PcDataSym *syms;
    uint32_t n_syms;
    const PcDataRun *runs;
    uint32_t n_runs;
    const PcDataFunc *funcs;
    uint32_t n_funcs;
    const PcDataName *names;
    uint32_t n_names;
    uint32_t heap_lo;           /* N64 pointers at or above this, outside the runtime images, are kept as is */
    PcDataState *state;
} PcDataTables;

/* Pointer resolution results, for the report. rdram: heap addresses kept as their value. raw: not an RDRAM
 * address (a ROM offset, a segment address), kept. unresolved: below the heap and in no symbol, kept. arena: an
 * arena's host address, which is the value itself for an arena at its N64 address. */
typedef struct {
    uint32_t null, own, cdef, arena, func, rdram, raw, unresolved, no_code, inexact, ambiguous;
    uint32_t addr_words;        /* non-pointer words that look like N64 addresses */
    uint32_t addr_words_host;   /* ... of host storage, a function or code, which the word can't reach */
} PcDataStats;

/* The loader (pc_data.c). load_all: inflate every image from the ROM and fill all of them, then write the
 * report (env PC_DATA_REPORT, else the build's data_report.txt; "" for none). 0 on success; -1 at once if an
 * arena at its N64 address is not mapped. load_image: fill one image again (an overlay load). report_path NULL
 * means the default. */
int pc_data_load_all(const PcDataTables *t, const uint8_t *rom, size_t rom_size, const char *report_path);
int pc_data_load_image(const PcDataTables *t, int image);
const PcDataStats *pc_data_stats(void);
/* Host address for an N64 address with the tables' symbols (NULL if none); for checks and tools. */
void *pc_data_host_of(const PcDataTables *t, uint32_t n64_addr, int image);

/* The game's tables (build/pc/pc-data/data.c). */
extern const PcDataTables pc_data_tables;

#ifdef __cplusplus
}
#endif

#endif
