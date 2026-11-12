/* port/ultra/misc.c: osInitialize and the low-memory globals, the CPU's cache, TLB, interrupt and FPU control
 * (no-ops natively), address conversion, a few libc pieces of the boot segment, logging and the fault handler
 * (docs/port/ULTRA.md). On Windows the fault handler and backtraces are in win32.c. */
#define _GNU_SOURCE
#ifndef _WIN32
#include <dlfcn.h>
#include <execinfo.h>
#include <fenv.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    pc_si_fault_flush();
    fflush(NULL);
    abort();
}

/* ---------------------------------------------------------------- osInitialize and the boot globals */

/* libultra's boot parameters the game reads, which the N64's boot code left in low memory for osInitialize: the TV
 * type (0 PAL, 1 NTSC, 2 MPAL: the PC is an NTSC console) and RDRAM's size in bytes. Ordinary variables, set by
 * pc_os_initialize; the game C declares them extern by their libultra names. */
uint32_t osTvType;
uint32_t osMemSize;

/* osMemSize's value. 4 MiB until main sets it: no Expansion Pak, so the game leaves its Expansion Pak flag
 * (expansion_pak_present) clear and skips overlay B and the extra waves in races. game.expansion_pak (on by default)
 * reports 8 MiB. game.memory reports its size, whatever the pak setting: the game's heap runs to osMemSize
 * (locate_main_heap: the heap's block, port/ultra/memory.c), and sets the Expansion Pak flag past 4 MiB, as a console
 * with that much memory would (docs/port/MEMORY.md 1). */
static uint32_t g_mem_size = N64_RDRAM_SIZE_NO_PAK;

void pc_os_initialize(void)
{
    osTvType = 1;   /* NTSC */
    osMemSize = g_mem_size;
}

uint32_t pc_os_mem_size(void) { return g_mem_size; }
void pc_os_set_expansion_pak(int on) { g_mem_size = on ? PC_RDRAM_SIZE : N64_RDRAM_SIZE_NO_PAK; }
void pc_os_set_mem_size(uint32_t bytes) { g_mem_size = bytes; }

/* main's first call (libultra's osInitialize; historical label). */
void __osInitialize_common(void) { pc_os_initialize(); }
void osInitialize(void) { pc_os_initialize(); }

/* ---------------------------------------------------------------- CPU control: nothing to do natively */

void osInvalDCache(void *p, int32_t n) { (void)p, (void)n; }
void osInvalICache(void *p, int32_t n) { (void)p, (void)n; }
void osWritebackDCache(void *p, int32_t n) { (void)p, (void)n; }
void osWritebackDCacheAll(void) {}
void osUnmapTLBAll(void) {}
void osMapTLBRdb(void) {}
uint32_t __osTLBLookup(void *vaddr) { return N64_TO_PHYS((uint32_t)(uintptr_t)vaddr); }

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

/* (osVirtualToPhysical and osPhysicalToVirtual: none, memory.c says why.) */

/* A generated stub was called (build/pc/pc-data/stubs.c, tools/port/gen_stubs.py). */
void pc_stub_hit(const char *name)
{
    fprintf(stderr, "rush2049pc: not implemented yet: %s (N64 thread %s, frame %u)\n", name, pc_os_thread_name(),
            pc_frame);
    pc_print_backtrace(1);
    pc_si_fault_flush();
    fflush(NULL);
    abort();
}

/* ---------------------------------------------------------------- faults */
void (*pc_on_fault)(const char *what);
void (*pc_on_exit)(int code);
#ifndef _WIN32

/* In a fault, the faulting instruction (fault): its frame is looked up at its own address, the others, return
 * addresses, 1 byte before (inside the call), as a fault on a function's first instruction would otherwise be put
 * in the function before. */
static uintptr_t g_fault_pc;

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
        uintptr_t pc = (uintptr_t)frames[i];
        if (!g_fault_pc || pc != g_fault_pc)
            pc--;
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

#ifdef PC_SANITIZE
/* A sanitizer build (docs/port/SANITIZE.md): the runtime's handlers, installed before main, get each fault after
 * this one names the N64 thread, and report it with their own stack. */
static struct sigaction g_prev_fault[NSIG];
#endif

static void fault(int sig, siginfo_t *si, void *ctx)
{
    const ucontext_t *uc = ctx;
#if defined(__x86_64__) && defined(REG_RIP)
    g_fault_pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
    g_fault_pc = (uintptr_t)uc->uc_mcontext.pc;
#else
    (void)uc;
#endif
    char line[256];
    int n = snprintf(line, sizeof line, "\nrush2049pc: %s at address %p in N64 thread %s\n", strsignal(sig),
                     si->si_addr, pc_os_thread_name());
    if (write(2, line, (size_t)n) < 0)
        _exit(128 + sig);
    char note[320];   /* the guard mode: which N64 address it was (docs/port/NOFIXED.md 5) */
    int k = sig == SIGSEGV || sig == SIGBUS ? pc_mem_fault_note((uintptr_t)si->si_addr, note, sizeof note) : 0;
    if (k > 0 && write(2, note, (size_t)k < sizeof note ? (size_t)k : sizeof note - 1) < 0)
        _exit(128 + sig);
#ifdef PC_SANITIZE
    const struct sigaction *prev = &g_prev_fault[sig];
    int chain = (prev->sa_flags & SA_SIGINFO) ? prev->sa_sigaction != NULL
                                              : prev->sa_handler != SIG_DFL && prev->sa_handler != SIG_IGN;
    if (!chain)
#endif
        pc_print_backtrace(1);
    pc_si_fault_flush();   /* the saves, if that is safe (si.c), before the log is closed */
    if (pc_on_fault && n > 13 && n < (int)sizeof line) {
        line[n - 1] = 0;          /* the line without its newlines */
        pc_on_fault(line + 13);   /* nor "\nrush2049pc: " */
    }
#ifdef PC_SANITIZE
    if (chain) {
        sigaction(sig, prev, NULL);
        if (si->si_code <= 0)   /* sent, not a fault: send it again for the runtime's handler */
            raise(sig);
        return;                 /* a fault: the instruction faults again, into the runtime's handler */
    }
#endif
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Per host thread: an alternate signal stack, and with RUSH_TRAP_FP set (glibc) a SIGFPE at the first invalid
 * floating-point operation or division by zero, which finds where a NaN or an infinity starts. */
void pc_fault_thread_init(void)
{
    static const size_t size = 64 << 10;
    stack_t ss = {.ss_sp = NULL, .ss_size = size};
#ifdef PC_SANITIZE
    stack_t cur;
    if (sigaltstack(NULL, &cur) == 0 && !(cur.ss_flags & SS_DISABLE))
        ss.ss_size = 0;   /* the runtime's own, which its handlers run on */
#endif
    if (ss.ss_size && (ss.ss_sp = malloc(size)) != NULL)
        sigaltstack(&ss, NULL);
#ifdef __GLIBC__
    if (getenv("RUSH_TRAP_FP"))
        feenableexcept(FE_INVALID | FE_DIVBYZERO);
#endif
}

void pc_fault_install(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
#ifdef PC_SANITIZE
    sigaction(SIGSEGV, &sa, &g_prev_fault[SIGSEGV]);
    sigaction(SIGBUS, &sa, &g_prev_fault[SIGBUS]);
    sigaction(SIGILL, &sa, &g_prev_fault[SIGILL]);
    sigaction(SIGFPE, &sa, &g_prev_fault[SIGFPE]);
#else
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
#endif
    pc_fault_thread_init();
}
#endif
