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

/* The writable data an image's own C defines, file-local and function-local statics included, as one host range
 * (the game's: port/data/overlays.ld gathers each runtime image's objects' .data and .bss). No table can name a
 * static, so the loader takes the range's bytes at startup and puts them back on every later load of the image, as
 * the N64's load of the image does. */
typedef struct {
    uint8_t image;
    uint8_t *start, *end;
} PcDataCRange;

/* A table that only pointers reach (port/data/reached.txt, docs/port/DATA.md 7): no symbol describes it, so without
 * a rule it would keep the ROM's byte order and the N64 layout. A root rule takes every pointer of a symbol with its
 * own storage (syms[sym], `slots` pointers); a field rule takes the pointer at slot_n64 (host: slot_host) of every
 * element its parent rule reaches. The count of elements a pointer reaches is fixed, ends at the element whose field
 * (field_n64, field_width bytes) is zero (that one included), is the parent element's field, or is element i of the
 * table at table_n64 for root slot i. A type with pointers, or with a host layout of its own, gets host storage
 * (`copy`) and the pointer is rewritten to it; any other is swapped in place in its arena. A pointer into one of the
 * `compatible` symbols, which hold that type already, resolves as usual. */
enum { PC_REACH_FIXED, PC_REACH_UNTIL, PC_REACH_PARENT, PC_REACH_TABLE };
typedef struct {
    const char *what;
    int32_t sym;                /* root rule: the table of pointers, else -1 */
    int32_t parent;             /* field rule: the rule that reaches the records holding the pointer, else -1 */
    uint32_t slot_n64, slot_host;
    uint32_t slots;
    uint32_t n64_size, host_size;   /* an element */
    uint32_t run_first, run_count;
    uint8_t count_kind;
    uint32_t count;
    uint32_t field_n64, field_width;
    uint32_t table_n64;         /* PC_REACH_TABLE: the N64 address of the table of counts */
    uint8_t copy;
    const uint32_t *compatible;
    uint32_t n_compatible;
} PcDataReach;

/* Game data the PC owns, with no N64 address (port/data/decoupled.txt, docs/port/DECOUPLE.md), whose N64 symbol
 * was initialized data (init=image): on each load of its image the loader fills the storage's first host_init bytes
 * from the image's n64_size bytes at n64_addr through the runs, and continues them to host_size with a period of
 * `period` host bytes (byte i is byte i - period), or zero (period 0). It is not in syms: no N64 address resolves to
 * it. */
typedef struct {
    const char *name;
    uint32_t n64_addr, n64_size;
    uint32_t host_size, host_init;
    void *host;
    uint32_t run_first, run_count;
    uint32_t period;
    uint8_t image;
} PcDataInit;

/* Mutable loader state (one per table set). */
typedef struct {
    const uint8_t *rom;
    size_t rom_size;
    uint8_t *image[8];          /* inflated images (raw images point into the ROM) */
    uint8_t *owned[8];          /* buffers to free */
    void **snapshot;            /* per symbol: startup bytes of PC_SYM_CDEF overlay data */
    void **crange_snapshot;     /* per C range: its startup bytes */
    int loaded;
    void *reached[8];           /* per image: the host storage its rules made on its last load (a list) */
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
    const PcDataCRange *cranges;   /* runtime images' C data, restored on their loads (may be none) */
    uint32_t n_cranges;
    const PcDataReach *reach;   /* tables only pointers reach (may be none) */
    uint32_t n_reach;
    const PcDataInit *inits;    /* decoupled data the images initialize (may be none) */
    uint32_t n_inits;
} PcDataTables;

/* Pointer resolution results, for the report. rdram: heap addresses kept as their value. raw: not an RDRAM
 * address (a ROM offset, a segment address), kept. unresolved: below the heap and in no symbol, kept. arena: an
 * arena's host address, which is the value itself for an arena at its N64 address; undescribed counts those whose
 * target is initialized data no declaration describes, which keeps the ROM's byte order (fine for bytes, wrong for
 * a table of wider values that is reached only through pointers). */
typedef struct {
    uint32_t null, own, cdef, arena, func, rdram, raw, unresolved, no_code, inexact, ambiguous;
    uint32_t addr_words;        /* non-pointer words that look like N64 addresses */
    uint32_t addr_words_host;   /* ... of host storage, a function or code, which the word can't reach */
    uint32_t undescribed;       /* arena pointers to initialized bytes no element describes: kept in N64 order */
    uint32_t text_in_words;     /* arena pointers to text inside an element wider than a byte, which swaps it */
    uint32_t reached, reached_elems;   /* tables the reach rules converted, and their elements */
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
