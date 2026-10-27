/* port/ultra/misc.c: osInitialize and the low-memory globals, the CPU's cache, TLB, interrupt and FPU control
 * (no-ops natively), address conversion, a few libc pieces of the boot segment, logging and the fault handler
 * (docs/port/ULTRA.md). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ultra.h"

int pc_verbose;

void pc_log(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "rush2049pc: [%s] %s\n", pc_os_thread_name(), buf);
}

void pc_fatal(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "rush2049pc: fatal: [%s] %s\n", pc_os_thread_name(), buf);
    fflush(NULL);
    abort();
}

/* ---------------------------------------------------------------- osInitialize and the boot globals */

/* The low-memory globals (N64 0x80000300-0x8000035C), set by the IPL and osInitialize. The game reads
 * osMemSize under its address name too (D_80000318: gen_stubs aliases it), and the values are mirrored into
 * the RDRAM window for code that reads the literal address. Weak, in case port/data defines them. */
#define WEAK __attribute__((weak))
WEAK uint32_t osTvType = 1;            /* NTSC */
WEAK uint32_t osRomType = 0;
WEAK uint32_t osRomBase = 0xB0000000u;
WEAK uint32_t osResetType = 0;         /* cold */
WEAK uint32_t osCicId = 6102;
WEAK uint32_t osVersion = 0;
/* 4 MiB by default: no Expansion Pak, so the game leaves its Expansion Pak flag (D_80156994) clear and skips
 * overlay B and the extra waves in races, the smaller first target docs/port/ASSETS.md suggests.
 * --expansion-pak reports 8 MiB, as N64ModernRuntime and v5 do (needed for parity runs against v5). The RDRAM
 * window is 8 MiB either way. */
WEAK uint32_t osMemSize = 0x00400000;
WEAK uint8_t osAppNMIBuffer[64];

static void put_word(uint32_t addr, uint32_t v)
{
    uint8_t *p = pc_n64_ptr(addr);
    if (p)
        memcpy(p, &v, 4);   /* native order: the game reads them as words */
}

void pc_os_initialize(void)
{
    if (osResetType == 0)
        memset(osAppNMIBuffer, 0, sizeof osAppNMIBuffer);
    put_word(0x80000300, osTvType);
    put_word(0x80000304, osRomType);
    put_word(0x80000308, osRomBase);
    put_word(0x8000030C, osResetType);
    put_word(0x80000310, osCicId);
    put_word(0x80000314, osVersion);
    put_word(0x80000318, osMemSize);
}

uint32_t pc_os_mem_size(void) { return osMemSize; }
void pc_os_set_expansion_pak(int on) { osMemSize = on ? PC_RDRAM_SIZE : 0x00400000; }

/* main's first call (libultra's osInitialize; historical label). */
void __osInitialize_common(void) { pc_os_initialize(); }
void osInitialize(void) { pc_os_initialize(); }

/* ---------------------------------------------------------------- CPU control: nothing to do natively */

void osInvalDCache(void *p, int32_t n) { (void)p, (void)n; }
void osInvalICache(void *p, int32_t n) { (void)p, (void)n; }
void osWritebackDCache(void *p, int32_t n) { (void)p, (void)n; }
void osWritebackDCacheAll(void) {}
void func_8002C5E0(void) {}   /* boot-data D-cache loop (cache 0xD) */
void osUnmapTLBAll(void) {}
void osMapTLBRdb(void) {}
uint32_t __osTLBLookup(void *vaddr) { return (uint32_t)(uintptr_t)vaddr & 0x1FFFFFFFu; }

static uint32_t g_sr = 0x2000FF01u, g_fpcsr = 0x01000800u, g_intmask = 0x003FFF01u;
uint32_t __osGetSR(void) { return g_sr; }
void __osSetSR(uint32_t sr) { g_sr = sr; }
uint32_t __osGetCause(void) { return 0; }
void __osSetCompare(uint32_t v) { (void)v; }
uint32_t __osSetFpcCsr(uint32_t v) { uint32_t old = g_fpcsr; g_fpcsr = v; return old; }
/* The game enables the FPU's invalid, divide-by-zero and overflow traps here; natively they stay masked. */
void __setfpcsr(uint32_t v) { g_fpcsr = v; }

/* One N64 thread runs at a time and the main thread's events are posted under the scheduler's lock, so
 * there is nothing to mask. */
uint32_t __osDisableInt(void) { return 1; }
void __osRestoreInt(uint32_t flags) { (void)flags; }
uint32_t osSetIntMask(uint32_t mask)
{
    uint32_t old = g_intmask;
    g_intmask = mask;
    return old;
}
void __osSetGlobalIntMask(uint32_t mask) { g_intmask |= mask; }
void __osResetGlobalIntMask(uint32_t mask) { g_intmask &= ~mask; }

/* ---------------------------------------------------------------- addresses */

/* RDRAM addresses become physical, as on the N64. A host address (the game's own code, data or stacks, all
 * above 4 GiB) doesn't fit the u32 the game stores it in: it is truncated, with one warning. The game's
 * framebuffers and loaded files live in the heap, inside the window. */
uint32_t osVirtualToPhysical(void *vaddr)
{
    uint32_t n64 = pc_n64_addr(vaddr);
    if (n64)
        return n64 & 0x1FFFFFFFu;
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
    void *p = paddr < PC_RDRAM_SIZE ? pc_n64_ptr(paddr) : NULL;
    return p ? p : (void *)(uintptr_t)paddr;
}

/* ---------------------------------------------------------------- faults */

/* The calling stack, symbolized with addr2line when it is installed: frames in the executable by their offset
 * from its load address (it is a PIE), others as library + offset. */
void pc_print_backtrace(int skip)
{
    void *frames[48];
    char cmd[8192], line[512];
    int n = backtrace(frames, 48), len = 0, i, k = 0;
    Dl_info self, d;
    const char *what[48];
    uintptr_t off[48];
    if (!dladdr((void *)pc_print_backtrace, &self))
        self.dli_fbase = NULL;
    for (i = skip + 1; i < n; i++, k++) {
        uintptr_t pc = (uintptr_t)frames[i] - 1;
        what[k] = NULL;
        off[k] = pc;
        if (dladdr((void *)pc, &d) && d.dli_fbase) {
            off[k] = pc - (uintptr_t)d.dli_fbase;
            what[k] = d.dli_fbase == self.dli_fbase ? NULL : d.dli_fname;
        }
    }
    FILE *p = NULL;
    if (self.dli_fbase) {
        len = snprintf(cmd, sizeof cmd, "addr2line -f -p -e /proc/%d/exe", (int)getpid());
        for (i = 0; i < k && len < (int)sizeof cmd - 24; i++)
            if (!what[i])
                len += snprintf(cmd + len, sizeof cmd - (size_t)len, " 0x%lx", (unsigned long)off[i]);
        snprintf(cmd + len, sizeof cmd - (size_t)len, " 2>/dev/null");
        p = popen(cmd, "r");
    }
    for (i = 0; i < k; i++) {
        if (what[i])
            fprintf(stderr, "  #%-2d %s+0x%lx\n", i + 1, what[i], (unsigned long)off[i]);
        else if (p && fgets(line, sizeof line, p))
            fprintf(stderr, "  #%-2d %s", i + 1, line);
        else
            fprintf(stderr, "  #%-2d 0x%lx\n", i + 1, (unsigned long)off[i]);
    }
    if (p)
        pclose(p);
}

/* A generated stub was called (build/pc/pc-data/stubs.c, tools/port/gen_stubs.py). */
void pc_stub_hit(const char *name)
{
    fprintf(stderr, "rush2049pc: not implemented yet: %s (N64 thread %s, frame %u)\n", name, pc_os_thread_name(),
            pc_frame);
    pc_print_backtrace(1);
    fflush(NULL);
    abort();
}

static void fault(int sig, siginfo_t *si, void *ctx)
{
    (void)ctx;
    char line[256];
    int n = snprintf(line, sizeof line, "\nrush2049pc: %s at address %p in N64 thread %s\n", strsignal(sig),
                     si->si_addr, pc_os_thread_name());
    if (write(2, line, (size_t)n) < 0)
        _exit(128 + sig);
    pc_print_backtrace(1);
    signal(sig, SIG_DFL);
    raise(sig);
}

void pc_fault_thread_init(void)
{
    static const size_t size = 64 << 10;
    stack_t ss = {.ss_sp = malloc(size), .ss_size = size};
    if (ss.ss_sp)
        sigaltstack(&ss, NULL);
}

void pc_fault_install(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    pc_fault_thread_init();
}
