/* port/ultra/memory.c: the game heap's block, the N64 guard, and the ranges of the files' data that the renderer and
 * the audio interpreter ask about (docs/port/MEMORY.md, docs/port/NATIVE.md 17).
 *
 * The game's memory is ordinary PC memory: its data and BSS are host storage (port/data), its display lists hold host
 * pointers, and its heap is the block mapped here, wherever the OS puts it. Nothing is mapped at an N64 address. The
 * N64 guard (RUSH2049_N64_GUARD=1, off by default) makes sure of that: it reserves the N64's address range with no
 * access, so that an N64 address anything still takes for a host pointer faults with a note saying what it was, and
 * it places the heap's block so that a heap pointer cut to 32 bits faults too. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "pc_game_types.h"   /* PC_HEAP_OFFSET: the heap's room */
#include "ultra.h"

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/lsan_interface.h>
#define PC_LSAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#include <sanitizer/lsan_interface.h>
#define PC_LSAN 1
#endif
#endif

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

/* ---------------------------------------------------------------- the N64 guard */

/* RUSH2049_N64_GUARD=1: the N64's CPU address range, KSEG0 and KSEG1 (where the game's RDRAM, the RCP's registers and
 * the cartridge were: 0x80000000 up to 0xC0000000), is reserved with no access, so a 32-bit N64 address that anything
 * still takes for a host pointer faults; the fault handler (misc.c) then prints pc_mem_fault_note's line. And the
 * heap's block is mapped where its low 32 bits fall in GUARD_TRAP, a range reserved with no access too: a heap pointer
 * cut to 32 bits faults there. Checked along every synthetic path (ctest n64_guard_*), and the lead's retail scripts. */
#define GUARD_LO 0x80000000u                 /* KSEG0's first address */
#define GUARD_HI 0xC0000000u                 /* KSEG2's: KSEG1 ends here */
#define GUARD_KSEG1 0xA0000000u              /* KSEG1's first address */
#define GUARD_TRAP 0x40000000u               /* the low 32 bits of the heap's mapping in the guard */
/* The heap's block keeps the N64 heap's place modulo HEAP_ALIGN (pc_mem_map_heap). */
#define HEAP_ALIGN 0x10000u
#define HEAP_LEAD (PC_HEAP_OFFSET & (HEAP_ALIGN - 1))
static int g_guard;
static uintptr_t g_trap_lo, g_trap_hi;       /* the trap's reserved range, empty without */

/* size bytes at addr reserved with no access; 0, or -1 when the range isn't free. */
static int reserve_none(uintptr_t addr, size_t size)
{
#ifdef _WIN32
    return VirtualAlloc((void *)addr, size, MEM_RESERVE, PAGE_NOACCESS) == (void *)addr ? 0 : -1;
#else
    void *p = mmap((void *)addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                   -1, 0);
    if (p != MAP_FAILED && (uintptr_t)p != addr) {   /* kernels before 4.17 take the flag as a hint */
        munmap(p, size);
        p = MAP_FAILED;
    }
    return p == MAP_FAILED ? -1 : 0;
#endif
}

int pc_mem_guard_init(void)
{
    const char *e = getenv("RUSH2049_N64_GUARD");
    if (!e || !*e || !strcmp(e, "0"))
        return 0;
    if (reserve_none(GUARD_LO, GUARD_HI - GUARD_LO) != 0) {
        fprintf(stderr, "rush2049pc: N64 guard: cannot reserve 0x%08X-0x%08X\n", GUARD_LO, GUARD_HI - 1);
        return -1;
    }
    g_guard = 1;
    fprintf(stderr, "rush2049pc: N64 guard (RUSH2049_N64_GUARD): 0x%08X-0x%08X reserved; an N64 address used as a host "
            "pointer faults there (docs/port/NATIVE.md 17)\n", GUARD_LO, GUARD_HI - 1);
    return 0;
}

int pc_mem_guard(void) { return g_guard; }

/* The weak reference to the data loader's names (port/data/pc_data.c): NULL in a program without it. */
void pc_data_describe_loaded(uint32_t n64, char *buf, size_t n) __attribute__((weak));
static uint8_t *g_heap;
static uint32_t g_heap_size;

int pc_mem_fault_note(uintptr_t a, char *buf, size_t n)
{
    char name[96] = "";
    if (!g_guard || !n)
        return 0;
    if (a >= GUARD_LO && a < GUARD_HI) {
        uint32_t v = (uint32_t)a;
        const char *what = v >= N64_CART_KSEG1 ? "the cartridge through KSEG1"
                         : v >= GUARD_KSEG1 ? "KSEG1: the RCP's registers, or RDRAM uncached" : "KSEG0: RDRAM";
        if (pc_data_describe_loaded && v < GUARD_KSEG1) {   /* the game's datum the N64 had there, if any */
            pc_data_describe_loaded(v, name, sizeof name);
            if (!strcmp(name, "?"))
                name[0] = 0;
        }
        return snprintf(buf, n, "rush2049pc: N64 guard: 0x%08X, an N64 address (%s)%s%s, used as a host pointer: "
                        "nothing is there on the PC (RUSH2049_N64_GUARD, docs/port/NATIVE.md 17)\n", v, what,
                        name[0] ? ", the N64's " : "", name);
    }
    if (a >= g_trap_lo && a < g_trap_hi)
        return snprintf(buf, n, "rush2049pc: N64 guard: 0x%08lX, a pointer into the game heap cut to 32 bits "
                        "(heap+0x%lX) (RUSH2049_N64_GUARD, docs/port/NATIVE.md 17)\n", (unsigned long)a,
                        (unsigned long)(a - g_trap_lo - HEAP_LEAD));
    return 0;
}

/* ---------------------------------------------------------------- the game heap's block */

/* The memory the game's main heap manages (port/ultra/heap.c, docs/port/MEMORY.md 1): one mapping of PC memory,
 * wherever the OS puts it, of osMemSize less PC_HEAP_OFFSET bytes, the N64's room for the heap at every game.memory.
 * The game rounds addresses it computes (the heap's 32-byte chunks, the frame buffers' 64 bytes), so the heap's first
 * byte keeps the N64's place modulo HEAP_ALIGN: as far past a 64 KiB boundary as 0x8017A640 was. On Linux the mapping
 * is sparse: a page takes memory once the game first writes it, so a large game.memory costs what the heap uses. On
 * Windows the block is committed (the commit charge, not RAM: pages are made as they are first touched), so a
 * game.memory the system can't commit fails here; top-down, above every allocation that could come before it. */

/* In the N64 guard: span bytes whose low 32 bits are GUARD_TRAP's (above 4 GiB, where the OS leaves the room), and the
 * trap reserved; NULL when no place is free (the guard runs without its trap). */
static uint8_t *map_heap_trapped(size_t span)
{
    static const uintptr_t highs[] = {0x55, 0x66, 0x44, 0x77};   /* below 2^39: every 64-bit host's space has these */
    for (size_t i = 0; i < sizeof highs / sizeof highs[0]; i++) {
        uintptr_t at = highs[i] << 32 | GUARD_TRAP;
        uint8_t *m;
#ifdef _WIN32
        m = VirtualAlloc((void *)at, span, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (m && (uintptr_t)m != at) {
            VirtualFree(m, 0, MEM_RELEASE);
            m = NULL;
        }
#else
        void *p = mmap((void *)at, span, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (p != MAP_FAILED && (uintptr_t)p != at) {
            munmap(p, span);
            p = MAP_FAILED;
        }
        m = p == MAP_FAILED ? NULL : p;
#endif
        if (!m)
            continue;
        if (reserve_none(GUARD_TRAP, span) == 0) {
            g_trap_lo = GUARD_TRAP;
            g_trap_hi = GUARD_TRAP + span;
        }
        return m;
    }
    return NULL;
}

int pc_mem_map_heap(uint32_t mem_size)
{
    uint32_t size = mem_size > PC_HEAP_OFFSET ? mem_size - PC_HEAP_OFFSET : 0;
    size_t span = (size_t)size + HEAP_LEAD + HEAP_ALIGN;
    uint8_t *m;
    if (g_heap)
        return 0;
    m = g_guard ? map_heap_trapped(span) : NULL;
    if (!m) {
#ifdef _WIN32
        m = VirtualAlloc(NULL, span, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);
        if (!m) {
            fprintf(stderr, "rush2049pc: cannot make the game heap's %u MiB (error %lu)\n", size >> 20,
                    (unsigned long)GetLastError());
            return -1;
        }
#else
        void *p = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (p == MAP_FAILED) {
            fprintf(stderr, "rush2049pc: cannot make the game heap's %u MiB (%s)\n", size >> 20, strerror(errno));
            return -1;
        }
        m = p;
#endif
    }
    g_heap = (uint8_t *)(((uintptr_t)m + HEAP_ALIGN - 1) & ~(uintptr_t)(HEAP_ALIGN - 1)) + HEAP_LEAD;
    g_heap_size = size;
#ifdef PC_LSAN
    /* Host allocations the game holds only in its heap are not leaks (docs/port/SANITIZE.md). LSan reads the whole
     * block at exit, which makes every page of a large game.memory resident then. */
    __lsan_register_root_region(g_heap, g_heap_size);
#endif
    if (g_guard)
        fprintf(stderr, "rush2049pc: N64 guard: the game heap's block at %p; %s\n", (void *)g_heap,
                g_trap_hi ? "a pointer into it cut to 32 bits faults" : "no trap for its pointers cut to 32 bits");
    else if (pc_verbose)
        pc_log("the game heap's block: %u bytes at %p", g_heap_size, (void *)g_heap);
    return 0;
}

uint8_t *pc_mem_heap(uint32_t *size)
{
    *size = g_heap_size;
    return g_heap;
}

/* ---------------------------------------------------------------- ranges of the files' data */

/* Two sets of address ranges that the converters and the PI DMA keep, and that the renderer and the audio interpreter
 * ask about: the bytes still in the N64's byte order (big-endian: texels, TLUTs, the sound's data), and the models'
 * vertices the WAD converter made native (port/assets/wad.c, docs/port/NATIVE.md step 10). Each set is sorted and
 * disjoint. A range that overlaps existing ones merges with them, so a PI DMA into a registered buffer (the MusyX
 * caches) leaves it one range; adjacent ranges stay apart, so each buffer can be dropped on its own when it is freed.
 * The converters run on the N64 threads and the renderer on the main thread: each set has a lock. */
typedef struct { uintptr_t start, end; } Range;
typedef struct {
    pthread_mutex_t lock;
    Range *r;
    size_t n, cap;
} RangeSet;

static RangeSet g_be = {PTHREAD_MUTEX_INITIALIZER, NULL, 0, 0};
static RangeSet g_model_data = {PTHREAD_MUTEX_INITIALIZER, NULL, 0, 0};

static void range_add(RangeSet *set, const void *start, size_t size)
{
    uintptr_t s = (uintptr_t)start, e = s + size;
    if (!size)
        return;
    pthread_mutex_lock(&set->lock);
    size_t i = 0, j;
    while (i < set->n && set->r[i].end <= s)
        i++;
    for (j = i; j < set->n && set->r[j].start < e; j++) {
        if (set->r[j].start < s)
            s = set->r[j].start;
        if (set->r[j].end > e)
            e = set->r[j].end;
    }
    if (j == i && set->n == set->cap) {
        set->cap = set->cap * 2 + 16;
        set->r = realloc(set->r, set->cap * sizeof *set->r);
    }
    if (j == i) {   /* no overlap: insert */
        memmove(&set->r[i + 1], &set->r[i], (set->n - i) * sizeof *set->r);
        set->n++;
    } else {        /* replace ranges i..j-1 with their union */
        memmove(&set->r[i + 1], &set->r[j], (set->n - j) * sizeof *set->r);
        set->n -= j - i - 1;
    }
    set->r[i] = (Range){s, e};
    pthread_mutex_unlock(&set->lock);
}

/* Drops the range that holds start (the buffer is freed, or refilled with other data). */
static void range_drop(RangeSet *set, const void *start)
{
    uintptr_t a = (uintptr_t)start;
    pthread_mutex_lock(&set->lock);
    for (size_t i = 0; i < set->n; i++)
        if (set->r[i].start <= a && a < set->r[i].end) {
            memmove(&set->r[i], &set->r[i + 1], (set->n - i - 1) * sizeof *set->r);
            set->n--;
            break;
        }
    pthread_mutex_unlock(&set->lock);
}

static int range_has(RangeSet *set, const void *p)
{
    uintptr_t a = (uintptr_t)p;
    int found;
    pthread_mutex_lock(&set->lock);
    size_t lo = 0, hi = set->n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (set->r[mid].end <= a)
            lo = mid + 1;
        else
            hi = mid;
    }
    found = lo < set->n && set->r[lo].start <= a;
    pthread_mutex_unlock(&set->lock);
    return found;
}

void pc_mem_register_be(const void *start, size_t size) { range_add(&g_be, start, size); }
void pc_mem_unregister_be(const void *start) { range_drop(&g_be, start); }
int pc_mem_is_be(const void *p) { return range_has(&g_be, p); }

void pc_mem_register_model_data(const void *start, size_t size) { range_add(&g_model_data, start, size); }
void pc_mem_unregister_model_data(const void *start) { range_drop(&g_model_data, start); }
int pc_mem_is_model_data(const void *p) { return range_has(&g_model_data, p); }
