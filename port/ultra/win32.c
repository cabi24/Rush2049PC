/* port/ultra/win32.c: the fault handler and backtraces on Windows, where misc.c has the POSIX ones
 * (docs/port/WINDOWS.md). Built only for Windows (CMakeLists.txt).
 *
 * An unhandled exception in any thread reaches the process's unhandled-exception filter, the counterpart of the
 * SIGSEGV handler: it prints the exception, the address and the N64 thread, and a backtrace unwound from the
 * exception's own context with the image's unwind tables (.pdata, which GCC writes for every function). Then the
 * process ends with the exception code, without Windows Error Reporting. Frames in the executable are symbolized
 * by addr2line when it is on PATH (MSYS2's, or mingw-w64's on the build machine), by their offset in .text, which
 * doesn't move with the image; the others are printed as module + offset. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ultra.h"

#define MAX_FRAMES 48

/* 1 if [p, p + n) is committed and readable: the unwinder reads saved return addresses through it. */
static int readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mi;
    if (!VirtualQuery(p, &mi, sizeof mi) || mi.State != MEM_COMMIT || (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return 0;
    return (const uint8_t *)p + n <= (const uint8_t *)mi.BaseAddress + mi.RegionSize;
}

/* The return addresses from ctx outwards, innermost first. A frame with no unwind entry is a leaf (or a call
 * through a bad pointer): its return address is on top of the stack. */
static int unwind(CONTEXT *ctx, void **pcs, int max)
{
    int n = 0;
    while (n < max && ctx->Rip) {
        DWORD64 base = 0, frame = 0;
        void *handler_data = NULL;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx->Rip, &base, NULL);
        pcs[n++] = (void *)(uintptr_t)ctx->Rip;
        if (fn) {
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx->Rip, fn, ctx, &handler_data, &frame, NULL);
        } else {
            if (!readable((void *)(uintptr_t)ctx->Rsp, 8))
                break;
            ctx->Rip = *(DWORD64 *)(uintptr_t)ctx->Rsp;
            ctx->Rsp += 8;
        }
    }
    if (n == 0 && max > 0 && readable((void *)(uintptr_t)ctx->Rsp, 8)) {   /* a call to address 0 */
        pcs[n++] = NULL;
        ctx->Rip = *(DWORD64 *)(uintptr_t)ctx->Rsp;
        ctx->Rsp += 8;
        n += unwind(ctx, pcs + 1, max - 1);
    }
    return n;
}

/* The module holding pc, and pc's offset from its .text, or -1 (no module or not in .text). */
static HMODULE module_of(const void *pc, uintptr_t *text_off)
{
    HMODULE m = NULL;
    *text_off = (uintptr_t)-1;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)pc, &m))
        return NULL;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)m;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const uint8_t *)m + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    uintptr_t rva = (uintptr_t)pc - (uintptr_t)m;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (!memcmp(sec[i].Name, ".text", 6) && rva >= sec[i].VirtualAddress &&
            rva < sec[i].VirtualAddress + sec[i].Misc.VirtualSize)
            *text_off = rva - sec[i].VirtualAddress;
    return m;
}

static void print_frames(void **pcs, int n)
{
    char exe[MAX_PATH], name[MAX_PATH], cmd[8192], line[512];
    HMODULE self = GetModuleHandleW(NULL), mod[MAX_FRAMES];
    uintptr_t off[MAX_FRAMES];
    int len, i;
    FILE *p = NULL;
    if (n > MAX_FRAMES)
        n = MAX_FRAMES;
    for (i = 0; i < n; i++)
        mod[i] = module_of(pcs[i] ? (const uint8_t *)pcs[i] - 1 : NULL, &off[i]);   /* -1: inside the call */
    if (GetModuleFileNameA(self, exe, sizeof exe)) {
        len = snprintf(cmd, sizeof cmd, "addr2line -f -p -e \"%s\" -j .text", exe);
        for (i = 0; i < n && len < (int)sizeof cmd - 24; i++)
            if (mod[i] == self && off[i] != (uintptr_t)-1)
                len += snprintf(cmd + len, sizeof cmd - (size_t)len, " 0x%llx", (unsigned long long)off[i]);
        snprintf(cmd + len, sizeof cmd - (size_t)len, " 2>NUL");
        p = _popen(cmd, "r");
    }
    for (i = 0; i < n; i++) {
        if (mod[i] == self && off[i] != (uintptr_t)-1 && p && fgets(line, sizeof line, p) && strncmp(line, "??", 2))
            fprintf(stderr, "  #%-2d %s", i + 1, line);
        else if (mod[i] && GetModuleFileNameA(mod[i], name, sizeof name)) {
            const char *base = strrchr(name, '\\');
            fprintf(stderr, "  #%-2d %s+0x%llx\n", i + 1, base ? base + 1 : name,
                    (unsigned long long)((uintptr_t)pcs[i] - (uintptr_t)mod[i]));
        } else
            fprintf(stderr, "  #%-2d %p\n", i + 1, pcs[i]);
    }
    if (p)
        _pclose(p);
}

void pc_print_backtrace(int skip)
{
    CONTEXT ctx;
    void *pcs[MAX_FRAMES + 8];
    RtlCaptureContext(&ctx);
    int n = unwind(&ctx, pcs, MAX_FRAMES + skip + 1);
    if (n > skip + 1)
        print_frames(pcs + skip + 1, n - skip - 1);
}

static const char *exception_name(DWORD code, const EXCEPTION_RECORD *er)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        if (er->NumberParameters >= 1)
            return er->ExceptionInformation[0] == 0 ? "Access violation (read)"
                   : er->ExceptionInformation[0] == 8 ? "Access violation (execute)"
                                                      : "Access violation (write)";
        return "Access violation";
    case EXCEPTION_IN_PAGE_ERROR: return "In-page error";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal instruction";
    case EXCEPTION_PRIV_INSTRUCTION: return "Privileged instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Integer division by zero";
    case EXCEPTION_INT_OVERFLOW: return "Integer overflow";
    case EXCEPTION_STACK_OVERFLOW: return "Stack overflow";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "Misaligned data";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array bounds exceeded";
    case EXCEPTION_BREAKPOINT: return "Breakpoint";
    default: return code >= EXCEPTION_FLT_DENORMAL_OPERAND && code <= EXCEPTION_FLT_UNDERFLOW
                        ? "Floating-point exception" : "Exception";
    }
}

static LONG WINAPI fault(EXCEPTION_POINTERS *ep)
{
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    void *addr = er->ExceptionAddress, *pcs[MAX_FRAMES];
    char line[256];
    DWORD written;
    if ((er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || er->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        er->NumberParameters >= 2)
        addr = (void *)er->ExceptionInformation[1];   /* the data address, as si_addr is */
    int n = snprintf(line, sizeof line, "\nrush2049pc: %s (0x%08lX) at address %p in N64 thread %s\n",
                     exception_name(er->ExceptionCode, er), (unsigned long)er->ExceptionCode, addr,
                     pc_os_thread_name());
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, (DWORD)n, &written, NULL);
    CONTEXT ctx = *ep->ContextRecord;
    int frames = unwind(&ctx, pcs, MAX_FRAMES);
    print_frames(pcs, frames);
    fflush(stderr);
    if (pc_on_fault && n > 13 && n < (int)sizeof line) {
        line[n - 1] = 0;          /* the line without its newlines */
        pc_on_fault(line + 13);   /* nor "\nrush2049pc: " */
    }
    return EXCEPTION_EXECUTE_HANDLER;   /* the process ends with the exception's code */
}

/* Room on each thread's stack for the filter after a stack overflow. */
void pc_fault_thread_init(void)
{
    ULONG size = 64 << 10;
    SetThreadStackGuarantee(&size);
}

void pc_fault_install(void)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(fault);
    pc_fault_thread_init();
}
