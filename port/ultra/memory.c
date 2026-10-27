/* port/ultra/memory.c: the RDRAM window, N64 address translation and big-endian ranges (docs/port/ULTRA.md).
 *
 * The game addresses RDRAM outside its own symbols, with 32-bit arithmetic: the heap from the end of the game
 * BSS (0x8017A640) to osMemSize | K0BASE, where every loaded file lands and is relocated in place with
 * `+= (u32)base` and `| 0x80000000` (docs/port/ASSETS.md); the low-memory globals at 0x80000300; the overlay
 * region at 0x8038A400. So an 8 MiB window is mapped at host 0x80000000 (KSEG0) and, from the same memory, at
 * 0xA0000000 (KSEG1): those N64 addresses are host addresses. The window is load-bearing, so on Linux the
 * fixed mapping is required; elsewhere pc_n64_ptr translates, and the game's literal addresses fault.
 *
 * Everything else the host allocates (the PIE image, the heap, the N64 threads' stacks) is above 4 GiB, so the
 * renderer can tell host pointers from N64 addresses. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ultra.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define K1_BASE 0xA0000000u

static uint8_t *g_rdram, *g_rdram_k1;
static int g_fixed;

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
    int fd = memfd_create("rush2049pc-rdram", 0);
    if (fd >= 0 && ftruncate(fd, PC_RDRAM_SIZE) != 0) {
        close(fd);
        fd = -1;
    }
    g_rdram = map_at(PC_RDRAM_BASE, PC_RDRAM_SIZE, fd);
    if (g_rdram && fd >= 0)
        g_rdram_k1 = map_at(K1_BASE, PC_RDRAM_SIZE, fd);
    if (g_rdram) {
        g_fixed = 1;
    } else {
#ifdef __linux__
        fprintf(stderr, "rush2049pc: cannot map the RDRAM window at host 0x%08X (%s); the game's heap must live "
                "there (docs/port/ULTRA.md)\n", PC_RDRAM_BASE, strerror(errno));
        return -1;
#else
        pc_log("cannot map the RDRAM window at 0x%08X (%s): translating N64 addresses instead",
               PC_RDRAM_BASE, strerror(errno));
        g_rdram = mmap(NULL, PC_RDRAM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_rdram == MAP_FAILED)
            return -1;
#endif
    }
    if (fd >= 0)
        close(fd);
    if (pc_verbose)
        pc_log("RDRAM window at %p%s", (void *)g_rdram, g_rdram_k1 ? ", KSEG1 alias at 0xA0000000" : "");
    return 0;
}

int pc_mem_fixed(void) { return g_fixed; }

/* ---------------------------------------------------------------- absolute symbols in the PIE */

/* gen_stubs puts data the game uses by its N64 address at that address (aliases.ld: the low-memory globals,
 * the heap start, cartridge offsets). binutils 2.40 on aarch64 gives the GOT slot of such an absolute symbol a
 * RELATIVE relocation, so at run time it holds the load address plus the value. Nothing the linker places has
 * a link-time address at 0x80000000 or above, so a RELATIVE relocation with an addend there is one of these:
 * put the value back. Runs before any game code. */
static int fix_absolute(struct dl_phdr_info *info, size_t size, void *count)
{
    (void)size;
    if (info->dlpi_name && info->dlpi_name[0])
        return 0;   /* the executable comes first, with no name */
    uintptr_t base = info->dlpi_addr, relro = 0, relro_size = 0;
    const ElfW(Dyn) *dyn = NULL;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        if (ph->p_type == PT_DYNAMIC)
            dyn = (const ElfW(Dyn) *)(base + ph->p_vaddr);
        else if (ph->p_type == PT_GNU_RELRO)
            relro = base + ph->p_vaddr, relro_size = ph->p_memsz;
    }
    uintptr_t rela = 0;
    size_t relasz = 0;
    for (; dyn && dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_RELA)
            rela = dyn->d_un.d_ptr;
        else if (dyn->d_tag == DT_RELASZ)
            relasz = dyn->d_un.d_val;
    }
    if (!rela || !base)
        return 1;
    if (rela < base)
        rela += base;   /* the dynamic loader leaves some ports' entries unrelocated */
#if defined(__aarch64__)
    const unsigned relative = R_AARCH64_RELATIVE;
#elif defined(__x86_64__)
    const unsigned relative = R_X86_64_RELATIVE;
#else
    const unsigned relative = ~0u;
#endif
    long page = sysconf(_SC_PAGESIZE);
    uintptr_t lo = relro & ~(uintptr_t)(page - 1), hi = (relro + relro_size + (uintptr_t)page - 1) & ~(uintptr_t)(page - 1);
    int opened = 0;
    for (const ElfW(Rela) *r = (const ElfW(Rela) *)rela; (uintptr_t)(r + 1) <= rela + relasz; r++) {
        uint64_t v = (uint64_t)r->r_addend;
        if (ELF64_R_TYPE(r->r_info) != relative || v < 0x80000000u || v >= 0xC0000000u)
            continue;
        uint64_t *slot = (uint64_t *)(base + r->r_offset);
        if (*slot != base + v)
            continue;
        if (!opened && relro_size && mprotect((void *)lo, hi - lo, PROT_READ | PROT_WRITE) == 0)
            opened = 1;
        *slot = v;
        ++*(int *)count;
    }
    if (opened)
        mprotect((void *)lo, hi - lo, PROT_READ);
    return 1;
}

int pc_mem_fix_absolute(void)
{
    int count = 0;
    dl_iterate_phdr(fix_absolute, &count);
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
    else if (addr < PC_RDRAM_SIZE)
        phys = addr;
    else
        return NULL;
    return phys < PC_RDRAM_SIZE && g_rdram ? g_rdram + phys : NULL;
}

uint32_t pc_n64_addr(const void *host)
{
    const uint8_t *p = host;
    if (g_rdram && p >= g_rdram && p < g_rdram + PC_RDRAM_SIZE)
        return 0x80000000u + (uint32_t)(p - g_rdram);
    if (g_rdram_k1 && p >= g_rdram_k1 && p < g_rdram_k1 + PC_RDRAM_SIZE)
        return 0x80000000u + (uint32_t)(p - g_rdram_k1);
    return 0;
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
