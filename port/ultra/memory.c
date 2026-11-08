/* port/ultra/memory.c: the RDRAM window, N64 address translation and big-endian ranges (docs/port/ULTRA.md).
 *
 * The game addresses RDRAM outside its own symbols, with 32-bit arithmetic: the heap from the end of the game
 * BSS (game_bss_end) to osMemSize | KSEG0, where every loaded file lands and is relocated in place with
 * `+= (u32)base` and `| KSEG0` (docs/port/ASSETS.md); libultra's boot parameters in low memory (N64_BOOT_PARAMS); the
 * runtime images' block (N64_OVERLAY_BASE). So an 8 MiB window is mapped at host PC_RDRAM_BASE (the N64's KSEG0) and,
 * from the same memory, at N64_KSEG1: those N64 addresses are host addresses (the numbers: pc_n64_map.h, the N64's
 * memory map). The game's pointer-free data and BSS live there too,
 * at their N64 addresses (port/data's arenas, docs/port/DATA.md). The window is load-bearing, so on Linux the
 * fixed mapping is required; elsewhere pc_n64_ptr translates, and the game's literal addresses and its data
 * fault (port/data refuses to fill an unmapped arena).
 *
 * Everything else the host allocates (the PIE image, the heap, the N64 threads' stacks) is above 4 GiB, so the
 * renderer can tell host pointers from N64 addresses.
 *
 * Windows (docs/port/WINDOWS.md): a pagefile-backed section viewed at both addresses (MapViewOfFileEx fails
 * rather than moving when the range is taken), and required as on Linux.
 *
 * RUSH2049_NOFIXED=guard (docs/port/NOFIXED.md 5, off by default; not on Windows): the window anywhere above 4 GiB,
 * KSEG0 and KSEG1 reserved with no access, pc_n64_ptr translating, so that every use of an N64 address as a
 * host pointer faults at once; the fault handler then says which N64 address it was (pc_mem_fault_note).
 *
 * A RUSH_NOFIXED build (cmake -DRUSH_NOFIXED=ON, docs/port/NOFIXED.md 2): the window is data.c's section pc_window,
 * wherever the executable loads, the game's names are inside it, and the old range is reserved as in the guard mode. */
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
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

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

static uint8_t *g_rdram, *g_rdram_k1;
static int g_fixed;
static int g_guard;        /* RUSH2049_NOFIXED=guard (below) */
static uint32_t g_cut, g_cut_size;   /* the guard mode: the window's host address cut to 32 bits, and its size there
                                      * when no N64 address can be in that range (else 0) */
static uint32_t g_size = PC_RDRAM_SIZE;   /* the window's bytes (game.memory) */

/* game.memory: the window's size, before pc_mem_init maps it. The N64's 8 MiB, or whole MiB from 16 MiB to
 * PC_RDRAM_MAX (docs/port/MEMORY.md). -1 for anything else, or once the window is mapped. */
int pc_mem_set_size(uint32_t bytes)
{
    if (g_rdram || (bytes != PC_RDRAM_SIZE && (bytes < PC_RCP_SEGMENT_SPAN || bytes > PC_RDRAM_MAX || (bytes & 0xFFFFF))))
        return -1;
    g_size = bytes;
    return 0;
}

uint32_t pc_rdram_size(void) { return g_size; }

#ifdef _WIN32
/* Reserves (without committing) every free range below 4 GiB but the window, so that whatever the process maps
 * from now on, the N64 threads' stacks and new heap segments among them, lands above 4 GiB. High-entropy ASLR puts
 * them there anyway, but not always: under gdb the heap and stacks started at a few MiB. */
static void reserve_low(void)
{
    MEMORY_BASIC_INFORMATION mi;
    const uintptr_t top = (uintptr_t)1 << 32, granule = 0x10000;
    for (uintptr_t a = granule; a < top; a = (uintptr_t)mi.BaseAddress + mi.RegionSize) {
        if (!VirtualQuery((void *)a, &mi, sizeof mi))
            break;
        uintptr_t lo = ((uintptr_t)mi.BaseAddress + granule - 1) & ~(granule - 1);
        uintptr_t hi = (uintptr_t)mi.BaseAddress + mi.RegionSize;
        if (mi.State == MEM_FREE && (hi = hi < top ? hi : top) > lo)
            VirtualAlloc((void *)lo, hi - lo, MEM_RESERVE, PAGE_NOACCESS);
    }
}

int pc_mem_init(void)
{
    const char *nofixed = getenv("RUSH2049_NOFIXED");
    if (nofixed && *nofixed)
        fprintf(stderr, "rush2049pc: RUSH2049_NOFIXED: no guard mode on Windows yet (docs/port/NOFIXED.md 5); the "
                "window is at 0x%08X as usual\n", PC_RDRAM_BASE);
    /* A pagefile-backed section commits its size up front (the commit charge, not RAM: pages are made as they are
     * first touched), so a game.memory the system can't commit fails here with ERROR_COMMITMENT_LIMIT. */
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, g_size, NULL);
    if (section) {
        g_rdram = MapViewOfFileEx(section, FILE_MAP_ALL_ACCESS, 0, 0, g_size, (void *)(uintptr_t)PC_RDRAM_BASE);
        if (g_rdram)
            g_rdram_k1 = MapViewOfFileEx(section, FILE_MAP_ALL_ACCESS, 0, 0, g_size, (void *)(uintptr_t)N64_KSEG1);
        CloseHandle(section);   /* the views keep it */
    }
    if (!g_rdram) {
        fprintf(stderr, "rush2049pc: cannot map the RDRAM window of %u MiB at host 0x%08X (error %lu); the game's "
                "heap must live there (docs/port/ULTRA.md)\n", g_size >> 20, PC_RDRAM_BASE,
                (unsigned long)GetLastError());
        return -1;
    }
    g_fixed = 1;
    reserve_low();
    /* Host data must be above 4 GiB, where high-entropy ASLR puts the heap and the stacks of a process whose
     * image asks for it (CMakeLists.txt): say so if what came before main got low memory anyway. */
    void *heap = malloc(64);
    int local;
    if (!((uintptr_t)heap >> 32) || !((uintptr_t)&local >> 32))
        fprintf(stderr, "rush2049pc: warning: host memory below 4 GiB (heap %p, stack %p); the renderer and the "
                "audio take addresses there for N64 ones\n", heap, (void *)&local);
    free(heap);
    if (pc_verbose && g_rdram_k1)
        pc_log("RDRAM window of %u MiB at %p, KSEG1 alias at 0x%08X", g_size >> 20, (void *)g_rdram, N64_KSEG1);
    else if (pc_verbose)
        pc_log("RDRAM window of %u MiB at %p", g_size >> 20, (void *)g_rdram);
    return 0;
}

/* The guard mode is POSIX only for now (docs/port/NOFIXED.md 5): RUSH2049_NOFIXED is ignored here. */
int pc_mem_guard(void) { return 0; }
int pc_mem_fault_note(uintptr_t a, char *buf, size_t n) { (void)a, (void)buf, (void)n; return 0; }
#else
/* Host allocations the game holds only in RDRAM are not leaks (docs/port/SANITIZE.md). LSan reads the whole region at
 * exit, which makes every page of a large game.memory resident then. */
static void lsan_root(void)
{
#ifdef PC_LSAN
    __lsan_register_root_region(g_rdram, g_size);
#endif
}

static void *map_at(uintptr_t addr, size_t size, int fd)
{
    int flags = (fd >= 0 ? MAP_SHARED : MAP_PRIVATE | MAP_ANONYMOUS) | MAP_FIXED_NOREPLACE;
    void *p = mmap((void *)addr, size, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (p == MAP_FAILED)
        return NULL;
    if ((uintptr_t)p != addr) {   /* kernels before 4.17 treat the flag as a hint */
        munmap(p, size);
        return NULL;
    }
    return p;
}

/* ---------------------------------------------------------------- the guard mode (docs/port/NOFIXED.md 5) */

/* RUSH2049_NOFIXED=guard. The old window's range, KSEG0 and KSEG1 (PC_RDRAM_BASE up to N64_KSEG2), is reserved with
 * no access, so a 32-bit N64 address the program still takes as a host pointer faults there. The window is an
 * anonymous mapping above 4 GiB, preferably at a host address whose low 32 bits are 0x40000000; that range below
 * 4 GiB is reserved too (the truncation trap), so a window address cut to 32 bits faults there as well. Nothing
 * is mapped at the KSEG1 view: K1 addresses are translated like K0 ones. */
#define GUARD_SPAN (N64_KSEG2 - N64_KSEG0)   /* KSEG0 and KSEG1: 1 GiB */
static uintptr_t g_trap;   /* the truncation trap's start, or 0 */

static void *map_none(uintptr_t addr, size_t size)
{
    void *p = mmap((void *)addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                   -1, 0);
    if (p != MAP_FAILED && (uintptr_t)p != addr) {
        munmap(p, size);
        p = MAP_FAILED;
    }
    return p == MAP_FAILED ? NULL : p;
}

/* The truncation trap for the window at g_rdram (above). */
static void guard_trap(void)
{
    uintptr_t low = (uintptr_t)g_rdram & 0xFFFFFFFFu;
    if (low && low + g_size <= PC_RDRAM_BASE && map_none(low, g_size))
        g_trap = low;
    g_cut = (uint32_t)low;
    g_cut_size = low >= g_size && low + g_size <= PC_RDRAM_BASE ? g_size : 0;
    g_guard = 1;
}

static int guard_init(void)
{
    if (!map_none(PC_RDRAM_BASE, GUARD_SPAN)) {
        fprintf(stderr, "rush2049pc: guard: cannot reserve 0x%08X-0x%08X (%s)\n", PC_RDRAM_BASE,
                PC_RDRAM_BASE + GUARD_SPAN - 1, strerror(errno));
        return -1;
    }
    /* Below 2^39, which every 64-bit Linux host's address space reaches. */
    static const uintptr_t hints[] = {0x5540000000u, 0x6640000000u, 0x4440000000u, 0x7740000000u};
    for (size_t i = 0; i < sizeof hints / sizeof hints[0] && !g_rdram; i++)
        g_rdram = map_at(hints[i], g_size, -1);
    if (!g_rdram) {
        void *p = mmap(NULL, g_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        g_rdram = p == MAP_FAILED ? NULL : p;
    }
    if (!g_rdram || !((uintptr_t)g_rdram >> 32)) {
        fprintf(stderr, "rush2049pc: guard: cannot map the RDRAM window of %u MiB above 4 GiB\n", g_size >> 20);
        return -1;
    }
    guard_trap();
    fprintf(stderr, "rush2049pc: guard (RUSH2049_NOFIXED=guard): the RDRAM window of %u MiB at %p, 0x%08X-0x%08X "
            "reserved, ", g_size >> 20, (void *)g_rdram, PC_RDRAM_BASE, PC_RDRAM_BASE + GUARD_SPAN - 1);
    if (g_trap)
        fprintf(stderr, "truncated window addresses trapped at 0x%08lX-0x%08lX (docs/port/NOFIXED.md 5)\n",
                (unsigned long)g_trap, (unsigned long)(g_trap + g_size - 1));
    else
        fprintf(stderr, "no trap for truncated window addresses (docs/port/NOFIXED.md 5)\n");
    return 0;
}

int pc_mem_guard(void) { return g_guard; }

#ifdef RUSH_NOFIXED
/* ---------------------------------------------------------------- no fixed address (docs/port/NOFIXED.md 2.1) */

/* The window is pc_window, the zero-fill section data.c defines in the executable (PC_RDRAM_MAX bytes, of which
 * game.memory's are the window); the game's names are inside it, and the tables give the arenas there. The old
 * window's range is reserved as in the guard mode, so that an N64 address still taken as a host pointer faults with a
 * note, and so is the truncation trap where the window's low 32 bits leave room for it. A program without data.c (a
 * test linking port/ultra alone) gets an anonymous mapping. pc_window_base (port/ultra/n64mem.c) is what the game C's
 * translations read (pc_prelude.h). */
extern uint8_t pc_window[] __attribute__((weak));
extern uint8_t *pc_window_base __attribute__((weak));

static int moved_init(void)
{
    if (pc_window) {
        g_rdram = pc_window;
    } else {
        void *p = mmap(NULL, g_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        g_rdram = p == MAP_FAILED ? NULL : p;
    }
    if (!g_rdram || !((uintptr_t)g_rdram >> 32)) {
        fprintf(stderr, "rush2049pc: cannot place the RDRAM window of %u MiB above 4 GiB (%p)\n", g_size >> 20,
                (void *)g_rdram);
        return -1;
    }
    if (&pc_window_base)
        pc_window_base = g_rdram;
    /* AddressSanitizer's shadow on x86-64 covers the range (docs/port/SANITIZE.md 1.1): then nothing is reserved. */
    int reserved = map_none(PC_RDRAM_BASE, GUARD_SPAN) != NULL;
    if (!reserved)
        pc_log("RUSH_NOFIXED: cannot reserve 0x%08X-0x%08X (%s): an N64 address taken as a host pointer won't fault "
               "there", PC_RDRAM_BASE, PC_RDRAM_BASE + GUARD_SPAN - 1, strerror(errno));
    guard_trap();
    g_guard = reserved;
    if (pc_verbose) {
        char span[48] = "";
        if (reserved)
            snprintf(span, sizeof span, ", 0x%08X-0x%08X reserved", PC_RDRAM_BASE, PC_RDRAM_BASE + GUARD_SPAN - 1);
        pc_log("RDRAM window of %u MiB at %p (RUSH_NOFIXED: pc_window)%s%s", g_size >> 20, (void *)g_rdram, span,
               g_trap ? ", the truncation trap" : "");
    }
    return 0;
}
#endif

/* The weak reference to the data loader's names (port/data/pc_data.c): NULL in a program without it. */
void pc_data_describe_loaded(uint32_t n64, char *buf, size_t n) __attribute__((weak));

/* In the guard mode, what a faulting address was: an N64 address taken as a host pointer, a window address cut to
 * 32 bits (in the trap, or wherever the window's low 32 bits put it when there is none), or one sign-extended from an
 * s32 (0xFFFFFFFF8xxxxxxx, a kernel address). 0 (and nothing written) otherwise, and always outside the guard mode. */
int pc_mem_fault_note(uintptr_t a, char *buf, size_t n)
{
    char name[96] = "";
    if (!g_guard || !n)
        return 0;
    uint32_t n64 = 0;
    const char *what = NULL;
    if (a >= PC_RDRAM_BASE && a < (uintptr_t)PC_RDRAM_BASE + GUARD_SPAN) {
        n64 = (uint32_t)a;
        what = n64 >= N64_CART_KSEG1 ? "a cartridge address (KSEG1 of the PI's range)"
             : n64 >= N64_KSEG1 ? "an N64 KSEG1 address" : "an N64 KSEG0 address";
    } else if (!(a >> 32) && a - g_cut < g_cut_size) {   /* in the trap, or where it would be */
        n64 = PC_RDRAM_BASE + (uint32_t)(a - g_cut);
        what = "a window address cut to 32 bits";
    } else if ((a >> 32) == 0xFFFFFFFFu && (uint32_t)a >= PC_RDRAM_BASE) {
        n64 = (uint32_t)a;
        what = "an N64 address sign-extended from an s32";
    } else {
        return 0;
    }
    uint32_t phys = N64_TO_PHYS(n64);
    if (pc_data_describe_loaded && n64 < N64_CART_KSEG1) {
        pc_data_describe_loaded(PC_RDRAM_BASE | phys, name, sizeof name);
        if (!strcmp(name, "?"))
            name[0] = 0;
    }
    return snprintf(buf, n, "rush2049pc: guard: %s, 0x%08X%s%s, used as a host pointer; the window has 0x%08X at %p "
                    "(docs/port/NOFIXED.md 5)\n", what, n64, name[0] ? ": " : "", name, PC_RDRAM_BASE | phys,
                    phys < g_size ? (void *)(g_rdram + phys) : NULL);
}

int pc_mem_init(void)
{
    const char *nofixed = getenv("RUSH2049_NOFIXED");
#ifdef RUSH_NOFIXED
    (void)nofixed;   /* the build is the moved one: RUSH2049_NOFIXED=guard is what it does anyway */
    return moved_init() == 0 ? (lsan_root(), 0) : -1;
#endif
    if (nofixed && *nofixed) {
        if (!strcmp(nofixed, "guard"))
            return guard_init() == 0 ? (lsan_root(), 0) : -1;
        fprintf(stderr, "rush2049pc: RUSH2049_NOFIXED takes guard, not '%s'; the window is at 0x%08X as usual\n",
                nofixed, PC_RDRAM_BASE);
    }
    /* A memfd is sparse: a page takes memory once the game first touches it, so a large game.memory costs only what
     * the heap uses. */
    int fd = memfd_create("rush2049pc-rdram", 0);
    if (fd >= 0 && ftruncate(fd, g_size) != 0) {
        close(fd);
        fd = -1;
    }
    g_rdram = map_at(PC_RDRAM_BASE, g_size, fd);
    if (g_rdram && fd >= 0)
        g_rdram_k1 = map_at(N64_KSEG1, g_size, fd);
    if (g_rdram) {
        g_fixed = 1;
    } else {
#ifdef __linux__
        fprintf(stderr, "rush2049pc: cannot map the RDRAM window of %u MiB at host 0x%08X (%s); the game's heap "
                "must live there (docs/port/ULTRA.md)\n", g_size >> 20, PC_RDRAM_BASE, strerror(errno));
        return -1;
#else
        pc_log("cannot map the RDRAM window at 0x%08X (%s): translating N64 addresses instead",
               PC_RDRAM_BASE, strerror(errno));
        g_rdram = mmap(NULL, g_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_rdram == MAP_FAILED) {
            g_rdram = NULL;
            return -1;
        }
#endif
    }
    if (fd >= 0)
        close(fd);
    lsan_root();
    if (pc_verbose && g_rdram_k1)
        pc_log("RDRAM window of %u MiB at %p, KSEG1 alias at 0x%08X", g_size >> 20, (void *)g_rdram, N64_KSEG1);
    else if (pc_verbose)
        pc_log("RDRAM window of %u MiB at %p", g_size >> 20, (void *)g_rdram);
    return 0;
}
#endif

int pc_mem_fixed(void) { return g_fixed; }

/* ---------------------------------------------------------------- absolute symbols in the PIE */

/* The GOT slots of symbols at absolute N64 addresses (port/ultra/absfix.c). */
int pc_mem_fix_absolute(void)
{
    int count = pc_fix_absolute_got();
    if (pc_verbose > 1)
        pc_log("%d absolute-symbol GOT slots fixed", count);
    return count;
}
uint8_t *pc_rdram(void) { return g_rdram; }

/* The guard mode: a value pc_n64_ptr gets that is the window's host address cut to 32 bits was a host pointer
 * stored in 32 bits, which no fault would show when the renderer or the audio takes it (docs/port/NOFIXED.md 5). */
#ifndef _WIN32
void pc_print_backtrace(int skip) __attribute__((weak));   /* misc.c: tests that link memory.c alone have none */
#endif
static void guard_cut(uint32_t addr)
{
    pc_log("guard: pc_n64_ptr(0x%08X): a window address cut to 32 bits (N64 0x%08X) taken for an N64 address "
           "(docs/port/NOFIXED.md 5)", addr, PC_RDRAM_BASE + (addr - g_cut));
#ifndef _WIN32
    if (pc_print_backtrace)
        pc_print_backtrace(1);
#endif
    fflush(NULL);
    abort();
}

void *pc_n64_ptr(uint32_t addr)
{
    uint32_t phys;
    if (addr - g_cut < g_cut_size)
        guard_cut(addr);
    if (N64_IS_KSEG0_OR_1(addr))
        phys = N64_TO_PHYS(addr);
    else if (addr < g_size)
        phys = addr;
    else
        return NULL;
    return phys < g_size && g_rdram ? g_rdram + phys : NULL;
}

uint32_t pc_n64_addr(const void *host)
{
    const uint8_t *p = host;
    if (g_rdram && p >= g_rdram && p < g_rdram + g_size)
        return N64_KSEG0 + (uint32_t)(p - g_rdram);
    if (g_rdram_k1 && p >= g_rdram_k1 && p < g_rdram_k1 + g_size)
        return N64_KSEG0 + (uint32_t)(p - g_rdram_k1);
    return 0;
}

/* ---------------------------------------------------------------- the RCP's addresses (docs/port/MEMORY.md 3) */

/* The rule past 16 MiB, and the display lists' relocation, are in rcpaddr.c (which tests link alone). */

/* RDRAM addresses become physical, as on the N64, below 16 MiB; past it, K0 (above). A host address (the game's own
 * code, data or stacks, all above 4 GiB) doesn't fit the u32 the game stores it in: it is truncated, with one
 * warning. The game's framebuffers and loaded files live in the heap, inside the window. */
uint32_t osVirtualToPhysical(void *vaddr)
{
    uint32_t n64 = pc_n64_addr(vaddr);
    if (n64)
        return N64_TO_PHYS(n64) < PC_RCP_SEGMENT_SPAN ? N64_TO_PHYS(n64) : n64;
    uintptr_t a = (uintptr_t)vaddr;
    if (a >> 32) {
        static int warned;
        if (!warned++)
            pc_log("osVirtualToPhysical(%p): host address above 4 GiB truncated", vaddr);
    }
    return (uint32_t)a;
}

void *osPhysicalToVirtual(uint32_t paddr)
{
    void *p = paddr < g_size ? pc_n64_ptr(paddr) : NULL;
    return p ? p : (void *)(uintptr_t)paddr;
}

/* ---------------------------------------------------------------- big-endian ranges */

typedef struct { uintptr_t start, end; } Range;
static pthread_mutex_t g_be_lock = PTHREAD_MUTEX_INITIALIZER;
static Range *g_be;
static size_t g_nbe, g_cap;

/* Sorted and disjoint. A range that overlaps existing ones merges with them, so a PI DMA into a registered
 * buffer (the MusyX caches) leaves it one range; adjacent ranges stay apart, so each buffer can be dropped on
 * its own when it is freed. */
void pc_mem_register_be(const void *start, size_t size)
{
    uintptr_t s = (uintptr_t)start, e = s + size;
    if (!size)
        return;
    pthread_mutex_lock(&g_be_lock);
    size_t i = 0, j;
    while (i < g_nbe && g_be[i].end <= s)
        i++;
    for (j = i; j < g_nbe && g_be[j].start < e; j++) {
        if (g_be[j].start < s)
            s = g_be[j].start;
        if (g_be[j].end > e)
            e = g_be[j].end;
    }
    if (j == i && g_nbe == g_cap) {
        g_cap = g_cap * 2 + 16;
        g_be = realloc(g_be, g_cap * sizeof *g_be);
    }
    if (j == i) {   /* no overlap: insert */
        memmove(&g_be[i + 1], &g_be[i], (g_nbe - i) * sizeof *g_be);
        g_nbe++;
    } else {        /* replace ranges i..j-1 with their union */
        memmove(&g_be[i + 1], &g_be[j], (g_nbe - j) * sizeof *g_be);
        g_nbe -= j - i - 1;
    }
    g_be[i] = (Range){s, e};
    pthread_mutex_unlock(&g_be_lock);
}

/* Drops the range that holds start (the buffer is freed or refilled with native data). */
void pc_mem_unregister_be(const void *start)
{
    uintptr_t a = (uintptr_t)start;
    pthread_mutex_lock(&g_be_lock);
    for (size_t i = 0; i < g_nbe; i++)
        if (g_be[i].start <= a && a < g_be[i].end) {
            memmove(&g_be[i], &g_be[i + 1], (g_nbe - i - 1) * sizeof *g_be);
            g_nbe--;
            break;
        }
    pthread_mutex_unlock(&g_be_lock);
}

int pc_mem_is_be(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    int found = 0;
    pthread_mutex_lock(&g_be_lock);
    size_t lo = 0, hi = g_nbe;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (g_be[mid].end <= a)
            lo = mid + 1;
        else
            hi = mid;
    }
    found = lo < g_nbe && g_be[lo].start <= a;
    pthread_mutex_unlock(&g_be_lock);
    return found;
}
