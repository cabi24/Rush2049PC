/* port/ultra/crossings.c: the crossing counter and the arena guard (docs/port/NATIVE.md 9). Built only with
 * RUSH_COUNT_CROSSINGS (cmake -DRUSH_COUNT_CROSSINGS=ON, a Debug build: tools/port/crossings.py makes one).
 *
 * The counter. Every crossing between a 32-bit N64 address and a host pointer (pc_crossings.h lists the kinds) is
 * counted by its site, a source line for the game C's translations or the calling code for the PC layer's, and by the
 * region the address lies in: an arena of the game's data, the heap, low memory, physical, a host pointer... At exit
 * the counts go to the file RUSH2049_CROSSINGS names ("%p" in it becomes the process id), one line per site with the
 * first address seen in each region and the datum it falls in; a line on stderr gives the totals. The PC layer's sites
 * are return addresses there (the executable's offset): tools/port/crossings.py names their functions and lines.
 *
 * The arena guard: RUSH2049_WINDOW_GUARD=arenas (stop at the first access) or arenas,count (count every access and go
 * on). Once the game's data is loaded, the boot and game arenas' pages in the RDRAM window (both views, KSEG0 and
 * KSEG1) are mapped without access, so whatever reaches that data by its N64 address faults:
 *   - stop: a note names the datum and the instruction, and the fault handler (misc.c) the N64 thread and the stack;
 *   - count: the fault is counted by its instruction and the page opened until the next graphics task, when every page
 *     closes again. So a run counts each page's first touch in a frame, which on the virtual clock (--fast) is the same
 *     in every run; an instruction that only ever touches a page some other one opened in that frame isn't seen.
 * A fault on a guarded page outside the arenas (the heap's first bytes share the game arena's last page) only opens
 * the page. The guard is paused while the data loader fills an image (pc_data.c). The runtime images' arenas are not
 * guarded: they lie in the heap block the game reserves for an image, which is heap memory (filled by the heap's free,
 * reused) whenever the image isn't loaded; the counter's ovl_a_arena and ovl_b_arena regions see their crossings.
 *
 * In the layout of today (port/data, docs/port/DATA.md 3) the arenas are at their N64 addresses, so every access by
 * name faults too: the guard proves it sees what it should. Once the arenas leave the window (NATIVE.md step 5), only
 * an N64 address that still reaches one does. POSIX only (mprotect and a SIGSEGV handler). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "pc_crossings.h"
#include "pc_data.h"
#include "ultra.h"

#ifndef RUSH_COUNT_CROSSINGS
#error "crossings.c is built only with RUSH_COUNT_CROSSINGS (CMakeLists.txt)"
#endif

static const char *const kind_names[PC_CROSS_KINDS] = {
    [PC_CROSS_N64P] = "PC_N64P",
    [PC_CROSS_N64A] = "PC_N64A",
    [PC_CROSS_N64W] = "PC_N64W",
    [PC_CROSS_SLOT_N64] = "PC_SLOT_N64",
    [PC_CROSS_SLOT_SET] = "PC_SLOT_SET",
    [PC_CROSS_STATLL_GET] = "pc_statll_link",
    [PC_CROSS_STATLL_SET] = "pc_statll_set_link",
    [PC_CROSS_N64_PTR] = "pc_n64_ptr",
    [PC_CROSS_N64_ADDR] = "pc_n64_addr",
    [PC_CROSS_VIRT_TO_PHYS] = "osVirtualToPhysical",
    [PC_CROSS_GFX_ROM_LIST] = "gfx.rom_list",
    [PC_CROSS_GFX_LIST] = "gfx.native_list",
    [PC_CROSS_AUDIO_N64PTR] = "audio.n64ptr",
    [PC_CROSS_AUDIO_FIELD] = "audio.field",
};

/* ---------------------------------------------------------------- regions: where an address points */

/* The data loader's tables (data.c), for the arenas and the heap's start, and its names for an address (pc_data.c):
 * absent in a program without them. */
extern const PcDataTables pc_data_tables __attribute__((weak));
void pc_data_describe_loaded(uint32_t n64, char *buf, size_t n) __attribute__((weak));

#define MAX_ARENAS 6
enum {
    R_NULL,
    R_LOW,          /* libultra's boot parameters, below the boot code */
    R_ARENA,        /* R_ARENA + i: arena i of the tables (boot, game, ovl_a, ovl_b) */
    R_IMAGE_BLOCK = R_ARENA + MAX_ARENAS,   /* the heap block reserved for the runtime images, outside their arenas */
    R_HEAP,
    R_WINDOW,       /* the window's other bytes: the N64's code ranges, where nothing lives on the PC */
    R_KSEG1,        /* RDRAM through KSEG1 */
    R_CART,         /* the cartridge through KSEG1 */
    R_PHYS,         /* a physical address (below 16 MiB) */
    R_SEGMENTED,    /* a segment's offset (segments 1-15) or physical past 16 MiB */
    R_OTHER,        /* anything else below 4 GiB */
    R_HOST,         /* a host pointer (at or above 4 GiB) */
    R_COUNT
};

static struct {
    int ready;
    unsigned n_arenas;
    uint32_t lo[MAX_ARENAS], hi[MAX_ARENAS];
    int guarded[MAX_ARENAS];   /* outside the heap: the boot and game arenas */
    char name[MAX_ARENAS][24];
    uint32_t heap_lo, window_end;
} g_map;

static void map_init(void)
{
    const PcDataTables *t = &pc_data_tables ? &pc_data_tables : NULL;
    g_map.window_end = N64_KSEG0 + pc_rdram_size();
    if (t) {
        g_map.heap_lo = t->heap_lo;
        for (uint32_t i = 0; i < t->n_arenas && g_map.n_arenas < MAX_ARENAS; i++) {
            const PcDataArena *ar = &t->arenas[i];
            unsigned k = g_map.n_arenas++;
            g_map.lo[k] = ar->n64_lo;
            g_map.hi[k] = ar->n64_hi;
            g_map.guarded[k] = ar->n64_lo < t->heap_lo;
            snprintf(g_map.name[k], sizeof g_map.name[k], "%s_arena",
                     ar->image < t->n_images ? t->images[ar->image].name : "image");
        }
    }
    g_map.ready = 1;
}

static const char *region_name(int r)
{
    static const char *const names[R_COUNT] = {
        [R_NULL] = "null", [R_LOW] = "low_memory", [R_IMAGE_BLOCK] = "image_block", [R_HEAP] = "heap",
        [R_WINDOW] = "window", [R_KSEG1] = "kseg1", [R_CART] = "cart", [R_PHYS] = "phys",
        [R_SEGMENTED] = "segmented", [R_OTHER] = "other", [R_HOST] = "host",
    };
    if (r >= R_ARENA && r < R_ARENA + MAX_ARENAS)
        return g_map.name[r - R_ARENA][0] ? g_map.name[r - R_ARENA] : "arena";
    return names[r];
}

/* A KSEG0 address's region. */
static int region_k0(uint32_t a)
{
    if (a < N64_LOW_MEMORY_END)
        return R_LOW;
    for (unsigned i = 0; i < g_map.n_arenas; i++)
        if (a >= g_map.lo[i] && a < g_map.hi[i])
            return R_ARENA + (int)i;
    if (a >= N64_OVERLAY_BASE && a < N64_OVERLAY_BLOCK_END)
        return R_IMAGE_BLOCK;
    if (a >= g_map.window_end)
        return R_OTHER;
    return g_map.heap_lo && a >= g_map.heap_lo ? R_HEAP : R_WINDOW;
}

static int region_of(unsigned long long v)
{
    if (v == 0)
        return R_NULL;
    if (v >> 32)
        return R_HOST;
    uint32_t a = (uint32_t)v;
    if (a >= N64_CART_KSEG1 && a < N64_KSEG2)
        return R_CART;
    if (N64_IS_KSEG1(a))
        return R_KSEG1;
    if (a >= N64_KSEG0 && a < N64_KSEG1)
        return region_k0(a);
    if (a < PC_RCP_SEGMENT_SPAN)
        return R_PHYS;
    return a < N64_CART_PHYS ? R_SEGMENTED : R_OTHER;
}

/* ---------------------------------------------------------------- the sites */

typedef struct {
    int kind;                 /* 0: a free entry */
    const char *file;         /* a source line's file, or NULL for a caller */
    uintptr_t key;            /* the line, or the caller's return address */
    unsigned long long n;
    unsigned long long by[R_COUNT];
    unsigned long long first[R_COUNT];   /* the first value seen in each region */
} Site;

#define SITES 8192   /* a power of two; about 250 are live along every synthetic path */
static Site g_sites[SITES];
static unsigned long long g_lost;   /* crossings with no entry left for their site */
static volatile int g_lock;

static void lock(volatile int *l)
{
    while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE))
        while (__atomic_load_n(l, __ATOMIC_RELAXED))
            ;
}

static void unlock(volatile int *l)
{
    __atomic_store_n(l, 0, __ATOMIC_RELEASE);
}

static void count(int kind, const char *file, uintptr_t key, unsigned long long v)
{
    if (kind <= 0 || kind >= PC_CROSS_KINDS)
        return;
    if (!v && (kind == PC_CROSS_AUDIO_N64PTR || kind == PC_CROSS_AUDIO_FIELD))
        return;   /* the audio interpreter's null pointers: no address */
    lock(&g_lock);
    if (!g_map.ready)
        map_init();
    int r = region_of(v);
    uintptr_t h = ((uintptr_t)file * 31u + key * 131u + (uintptr_t)kind * 7919u) & (SITES - 1);
    for (int i = 0; i < SITES; i++, h = (h + 1) & (SITES - 1)) {
        Site *s = &g_sites[h];
        if (!s->kind) {
            s->kind = kind;
            s->file = file;
            s->key = key;
        } else if (s->kind != kind || s->file != file || s->key != key) {
            continue;
        }
        if (!s->by[r]++)
            s->first[r] = v;
        s->n++;
        unlock(&g_lock);
        return;
    }
    g_lost++;
    unlock(&g_lock);
}

void pc_cross_at(int kind, unsigned long long value, const char *file, int line)
{
    count(kind, file, (uintptr_t)line, value);
}

void pc_cross_by(int kind, unsigned long long value, const void *caller)
{
    count(kind, NULL, (uintptr_t)caller, value);
}

/* ---------------------------------------------------------------- the arena guard */

typedef struct {
    uintptr_t lo, hi;    /* host addresses, whole pages */
    uint32_t n64;        /* the N64 address of lo */
} Span;

typedef struct {
    uintptr_t ip;        /* the faulting instruction */
    unsigned long long n;
    uint32_t first;      /* the first N64 address it touched */
} Fault;

#define MAX_SPANS (2 * MAX_ARENAS)   /* each guarded arena in both views */
#define FAULT_SITES 8192             /* a power of two */
static Span g_spans[MAX_SPANS];
static int g_nspans;
static uintptr_t g_page;
static int g_guard;                  /* 0 off, 1 stop at the first fault, 2 count */
static volatile int g_active;        /* the pages are to be closed (not paused, not finished) */
static volatile int g_paused;
static volatile int g_opened;        /* a page was opened since they were last closed */
static Fault g_faults[FAULT_SITES];
static volatile int g_fault_lock;
static unsigned long long g_fault_total, g_fault_lost, g_neighbour, g_closes;
static struct sigaction g_prev;

static void protect_all(int prot)
{
    for (int i = 0; i < g_nspans; i++)
        mprotect((void *)g_spans[i].lo, g_spans[i].hi - g_spans[i].lo, prot);
}

/* The N64 address a guarded host address stands for, or 0. */
static uint32_t guarded_n64(uintptr_t a)
{
    for (int i = 0; i < g_nspans; i++)
        if (a >= g_spans[i].lo && a < g_spans[i].hi)
            return g_spans[i].n64 + (uint32_t)(a - g_spans[i].lo);
    return 0;
}

static int in_guarded_arena(uint32_t n64)
{
    for (unsigned i = 0; i < g_map.n_arenas; i++)
        if (g_map.guarded[i] && n64 >= g_map.lo[i] && n64 < g_map.hi[i])
            return 1;
    return 0;
}

static uintptr_t fault_ip(const void *ctx)
{
    const ucontext_t *uc = ctx;
#if defined(__x86_64__) && defined(REG_RIP)
    return (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
    return (uintptr_t)uc->uc_mcontext.pc;
#else
    (void)uc;
    return 0;
#endif
}

static void record_fault(uintptr_t ip, uint32_t n64)
{
    lock(&g_fault_lock);
    g_fault_total++;
    uintptr_t h = (ip * 2654435761u) & (FAULT_SITES - 1);
    for (int i = 0; i < FAULT_SITES; i++, h = (h + 1) & (FAULT_SITES - 1)) {
        Fault *f = &g_faults[h];
        if (!f->ip) {
            f->ip = ip;
            f->first = n64;
        } else if (f->ip != ip) {
            continue;
        }
        f->n++;
        unlock(&g_fault_lock);
        return;
    }
    g_fault_lost++;
    unlock(&g_fault_lock);
}

/* Where code is: the executable's (or a library's) file name and the offset in it, for the note and the dump. */
static void where(uintptr_t pc, char *buf, size_t n)
{
    Dl_info d;
    if (dladdr((void *)pc, &d) && d.dli_fbase) {
        const char *base = d.dli_fname ? strrchr(d.dli_fname, '/') : NULL;
        snprintf(buf, n, "%s+0x%lx", base ? base + 1 : d.dli_fname ? d.dli_fname : "?",
                 (unsigned long)(pc - (uintptr_t)d.dli_fbase));
    } else {
        snprintf(buf, n, "0x%lx", (unsigned long)pc);
    }
}

static void on_fault(int sig, siginfo_t *si, void *ctx)
{
    uintptr_t a = (uintptr_t)si->si_addr;
    uint32_t n64 = g_guard ? guarded_n64(a) : 0;
    if (!n64) {   /* not the guard's: the fault handler (misc.c) reports it */
        if (g_prev.sa_flags & SA_SIGINFO)
            g_prev.sa_sigaction(sig, si, ctx);
        else if (g_prev.sa_handler != SIG_DFL && g_prev.sa_handler != SIG_IGN)
            g_prev.sa_handler(sig);
        else {
            signal(sig, SIG_DFL);
            raise(sig);
        }
        return;
    }
    int counted = g_active && !g_paused && in_guarded_arena(n64);
    int stray = g_active && !g_paused && !counted && !(g_map.heap_lo && n64 >= g_map.heap_lo);
    if ((counted || stray) && g_guard == 1) {   /* stop: say what it was, then the fault handler's report */
        char note[400], name[96] = "?", at[160];
        if (pc_data_describe_loaded)
            pc_data_describe_loaded(n64, name, sizeof name);
        where(fault_ip(ctx), at, sizeof at);
        int k = snprintf(note, sizeof note, "rush2049pc: window guard: %s 0x%08X (%s, the %s) reached by its N64 "
                         "address, by the instruction at %s (RUSH2049_WINDOW_GUARD=arenas, docs/port/NATIVE.md 9)\n",
                         counted ? "the datum at" : "the window's unused", n64, name, region_name(region_of(n64)), at);
        if (k > 0 && write(2, note, (size_t)k < sizeof note ? (size_t)k : sizeof note - 1) < 0)
            _exit(128 + sig);
        g_guard = 0;
        if (g_prev.sa_flags & SA_SIGINFO)
            g_prev.sa_sigaction(sig, si, ctx);
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    if (counted || stray)
        record_fault(fault_ip(ctx), n64);
    else if (g_active && !g_paused)
        __atomic_add_fetch(&g_neighbour, 1, __ATOMIC_RELAXED);
    mprotect((void *)(a & ~(g_page - 1)), g_page, PROT_READ | PROT_WRITE);
    g_opened = 1;
}

/* The spans of one arena, in both views of the window (pc_rdram(): KSEG0's host; KSEG1's is at its own address when
 * the window is the fixed one). */
static void add_spans(uint32_t lo, uint32_t hi)
{
    uint8_t *rdram = pc_rdram();
    uint32_t plo = lo & ~(uint32_t)(g_page - 1), phi = (hi + (uint32_t)g_page - 1) & ~(uint32_t)(g_page - 1);
    if (!rdram || g_nspans + 2 > MAX_SPANS)
        return;
    uintptr_t k0 = (uintptr_t)rdram + N64_TO_PHYS(plo);
    g_spans[g_nspans++] = (Span){k0, k0 + (phi - plo), plo};
    if (pc_mem_fixed()) {
        uintptr_t k1 = (uintptr_t)N64_KSEG1 + N64_TO_PHYS(plo);
        g_spans[g_nspans++] = (Span){k1, k1 + (phi - plo), plo};
    }
}

static void guard_start(const char *mode)
{
    if (!strcmp(mode, "arenas"))
        g_guard = 1;
    else if (!strcmp(mode, "arenas,count"))
        g_guard = 2;
    else {
        fprintf(stderr, "rush2049pc: RUSH2049_WINDOW_GUARD takes arenas or arenas,count, not '%s': no guard\n", mode);
        return;
    }
    g_page = (uintptr_t)sysconf(_SC_PAGESIZE);
    for (unsigned i = 0; i < g_map.n_arenas; i++)
        if (g_map.guarded[i])
            add_spans(g_map.lo[i], g_map.hi[i]);
    if (!g_nspans) {
        fprintf(stderr, "rush2049pc: window guard: no arena to guard (no data tables, or no window)\n");
        g_guard = 0;
        return;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &g_prev);
    g_active = 1;
    protect_all(PROT_NONE);
    fprintf(stderr, "rush2049pc: window guard (RUSH2049_WINDOW_GUARD=%s): the boot and game arenas' pages in the "
            "window without access, %s (docs/port/NATIVE.md 9)\n", mode,
            g_guard == 1 ? "the first access stops the program" : "every page's first access in a frame counted");
}

void pc_cross_frame(void)
{
    if (!g_active || g_paused || !g_opened)
        return;
    g_opened = 0;
    g_closes++;
    protect_all(PROT_NONE);
}

int pc_cross_guard_pause(void)
{
    if (!g_guard)
        return 0;
    if (__atomic_add_fetch(&g_paused, 1, __ATOMIC_ACQ_REL) == 1)
        protect_all(PROT_READ | PROT_WRITE);
    return 1;
}

void pc_cross_guard_resume(int *token)
{
    if (!*token || !g_guard)
        return;
    if (__atomic_sub_fetch(&g_paused, 1, __ATOMIC_ACQ_REL) == 0 && g_active)
        protect_all(PROT_NONE);
}

/* ---------------------------------------------------------------- start and finish */

void pc_crossings_start(void)
{
    lock(&g_lock);
    if (!g_map.ready)
        map_init();
    unlock(&g_lock);
    const char *g = getenv("RUSH2049_WINDOW_GUARD");
    if (g && *g)
        guard_start(g);
}

/* One region's count, its first value and, for RDRAM outside the heap, the datum there ("name+0xOFFSET", spaces made
 * underscores). */
static void print_region(FILE *f, int r, unsigned long long n, unsigned long long v)
{
    char name[96] = "";
    int rdram = r == R_LOW || (r >= R_ARENA && r < R_ARENA + MAX_ARENAS) || r == R_IMAGE_BLOCK || r == R_WINDOW
                || r == R_KSEG1;
    if (pc_data_describe_loaded && rdram) {
        uint32_t a = (uint32_t)v;
        pc_data_describe_loaded(N64_IS_KSEG1(a) && a < N64_CART_KSEG1 ? N64_KSEG1_TO_KSEG0(a) : a, name, sizeof name);
        for (char *c = name; *c; c++)
            if (*c == ' ')
                *c = '_';
        if (!strcmp(name, "?"))
            name[0] = 0;
    }
    fprintf(f, " %s=%llu@0x%llx%s%s", region_name(r), n, v, name[0] ? ":" : "", name);
}

static void dump(FILE *f)
{
    char exe[512] = "";
    ssize_t k = readlink("/proc/self/exe", exe, sizeof exe - 1);
    exe[k > 0 ? k : 0] = 0;
    fprintf(f, "# rush2049pc crossings: site KIND WHERE COUNT REGION=COUNT@FIRST[:DATUM]... (port/ultra/crossings.c, "
               "docs/port/NATIVE.md 9)\nexe %s\n", exe);
    for (int i = 0; i < SITES; i++) {
        const Site *s = &g_sites[i];
        if (!s->kind)
            continue;
        char at[512];
        if (s->file)
            snprintf(at, sizeof at, "%s:%lu", s->file, (unsigned long)s->key);
        else
            where(s->key - 1, at, sizeof at);   /* the call instruction, not the one after it */
        fprintf(f, "site %s %s %llu", kind_names[s->kind], at, s->n);
        for (int r = 0; r < R_COUNT; r++)
            if (s->by[r])
                print_region(f, r, s->by[r], s->first[r]);
        fputc('\n', f);
    }
    if (g_lost)
        fprintf(f, "lost %llu\n", g_lost);
    if (!g_guard)
        return;
    fprintf(f, "guard %s faults=%llu neighbour=%llu closes=%llu lost=%llu\n", g_guard == 1 ? "arenas" : "arenas,count",
            g_fault_total, g_neighbour, g_closes, g_fault_lost);
    for (int i = 0; i < FAULT_SITES; i++) {
        const Fault *ft = &g_faults[i];
        if (!ft->ip)
            continue;
        char at[512];
        where(ft->ip, at, sizeof at);
        fprintf(f, "fault %s %llu", at, ft->n);
        print_region(f, region_of(ft->first), ft->n, ft->first);
        fputc('\n', f);
    }
}

void pc_crossings_finish(void)
{
    if (g_guard) {   /* the N64 threads go on until the exit: nothing faults from here on */
        g_active = 0;
        protect_all(PROT_READ | PROT_WRITE);
    }
    lock(&g_lock);
    unsigned long long total[PC_CROSS_KINDS] = {0};
    for (int i = 0; i < SITES; i++)
        if (g_sites[i].kind)
            total[g_sites[i].kind] += g_sites[i].n;
    char line[1024] = "";
    int n = 0;
    for (int k = 1; k < PC_CROSS_KINDS && n < (int)sizeof line; k++)
        if (total[k])
            n += snprintf(line + n, sizeof line - (size_t)n, " %s %llu", kind_names[k], total[k]);
    fprintf(stderr, "rush2049pc: crossings:%s\n", n ? line : " none");
    if (g_guard)
        fprintf(stderr, "rush2049pc: window guard: %llu faults in the arenas, %llu on their pages' other bytes\n",
                g_fault_total, g_neighbour);
    const char *path = getenv("RUSH2049_CROSSINGS");
    if (path && *path) {
        char file[1024];
        const char *p = strstr(path, "%p");
        if (p)
            snprintf(file, sizeof file, "%.*s%d%s", (int)(p - path), path, (int)getpid(), p + 2);
        else
            snprintf(file, sizeof file, "%s", path);
        FILE *f = fopen(file, "w");
        if (f) {
            dump(f);
            fclose(f);
        } else {
            fprintf(stderr, "rush2049pc: crossings: cannot write %s (%s)\n", file, strerror(errno));
        }
    } else {
        fprintf(stderr, "rush2049pc: crossings: RUSH2049_CROSSINGS=FILE writes every site; tools/port/crossings.py "
                "--report FILE names them (docs/port/NATIVE.md 9)\n");
    }
    unlock(&g_lock);
}
