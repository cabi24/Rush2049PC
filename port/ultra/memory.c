/* port/ultra/memory.c: the RDRAM window, N64 address translation and big-endian ranges (docs/port/ULTRA.md).
 *
 * The game addresses RDRAM outside its own symbols, with 32-bit arithmetic: the heap from the end of the game
 * BSS (0x8017A640) to osMemSize | K0BASE, where every loaded file lands and is relocated in place with
 * `+= (u32)base` and `| 0x80000000` (docs/port/ASSETS.md); the low-memory globals at 0x80000300; the overlay
 * region at 0x8038A400. So an 8 MiB window is mapped at host 0x80000000 (KSEG0) and, from the same memory, at
 * 0xA0000000 (KSEG1): those N64 addresses are host addresses. The game's pointer-free data and BSS live there too,
 * at their N64 addresses (port/data's arenas, docs/port/DATA.md). The window is load-bearing, so on Linux the
 * fixed mapping is required; elsewhere pc_n64_ptr translates, and the game's literal addresses and its data
 * fault (port/data refuses to fill an unmapped arena).
 *
 * Everything else the host allocates (the PIE image, the heap, the N64 threads' stacks) is above 4 GiB, so the
 * renderer can tell host pointers from N64 addresses.
 *
 * Windows (docs/port/WINDOWS.md): a pagefile-backed section viewed at both addresses (MapViewOfFileEx fails
 * rather than moving when the range is taken), and required as on Linux. */
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

#define K1_BASE 0xA0000000u

static uint8_t *g_rdram, *g_rdram_k1;
static int g_fixed;
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
    /* A pagefile-backed section commits its size up front (the commit charge, not RAM: pages are made as they are
     * first touched), so a game.memory the system can't commit fails here with ERROR_COMMITMENT_LIMIT. */
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, g_size, NULL);
    if (section) {
        g_rdram = MapViewOfFileEx(section, FILE_MAP_ALL_ACCESS, 0, 0, g_size, (void *)(uintptr_t)PC_RDRAM_BASE);
        if (g_rdram)
            g_rdram_k1 = MapViewOfFileEx(section, FILE_MAP_ALL_ACCESS, 0, 0, g_size, (void *)(uintptr_t)K1_BASE);
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
    if (pc_verbose)
        pc_log("RDRAM window of %u MiB at %p%s", g_size >> 20, (void *)g_rdram,
               g_rdram_k1 ? ", KSEG1 alias at 0xA0000000" : "");
    return 0;
}
#else
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

int pc_mem_init(void)
{
    /* A memfd is sparse: a page takes memory once the game first touches it, so a large game.memory costs only what
     * the heap uses. */
    int fd = memfd_create("rush2049pc-rdram", 0);
    if (fd >= 0 && ftruncate(fd, g_size) != 0) {
        close(fd);
        fd = -1;
    }
    g_rdram = map_at(PC_RDRAM_BASE, g_size, fd);
    if (g_rdram && fd >= 0)
        g_rdram_k1 = map_at(K1_BASE, g_size, fd);
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
#ifdef PC_LSAN
    /* Host allocations the game holds only in RDRAM are not leaks (docs/port/SANITIZE.md). LSan reads the whole
     * region at exit, which makes every page of a large game.memory resident then. */
    __lsan_register_root_region(g_rdram, g_size);
#endif
    if (pc_verbose)
        pc_log("RDRAM window of %u MiB at %p%s", g_size >> 20, (void *)g_rdram,
               g_rdram_k1 ? ", KSEG1 alias at 0xA0000000" : "");
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

void *pc_n64_ptr(uint32_t addr)
{
    uint32_t phys;
    if (addr >= 0x80000000u && addr < 0xC0000000u)
        phys = addr & 0x1FFFFFFFu;
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
        return 0x80000000u + (uint32_t)(p - g_rdram);
    if (g_rdram_k1 && p >= g_rdram_k1 && p < g_rdram_k1 + g_size)
        return 0x80000000u + (uint32_t)(p - g_rdram_k1);
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
        return (n64 & 0x1FFFFFFFu) < PC_RCP_SEGMENT_SPAN ? n64 & 0x1FFFFFFFu : n64;
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
