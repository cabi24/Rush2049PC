/* port/ultra/win32.c: the fault handler and backtraces on Windows, where misc.c has the POSIX ones
 * (docs/port/WINDOWS.md). Built only for Windows (CMakeLists.txt).
 *
 * An unhandled exception in any thread reaches the process's unhandled-exception filter, the counterpart of the
 * SIGSEGV handler: it prints the exception, the address and the N64 thread, and a backtrace unwound from the
 * exception's own context with the image's unwind tables (.pdata, which GCC writes for every function). Then the
 * process ends with the exception code, without Windows Error Reporting. Frames in the executable are symbolized
 * by addr2line when it is on PATH (MSYS2's, or mingw-w64's on the build machine), by their offset in .text, which
 * doesn't move with the image; the others are printed as module + offset.
 *
 * The console's close (its window closed, a logoff or a shutdown while the program has a console) ends the process
 * when the handler returns, and at the latest 5 s after: the handler stops the main loop (pc_quit, as SIGTERM does)
 * and waits while it writes the saves (pc_si_finish); main's _exit then ends the process first. A loop that doesn't
 * stop in 3 s (held elsewhere) gets the saves written from the handler itself. Ctrl-C and Ctrl-Break are left to
 * the C runtime's signals (loop.c). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ultra.h"

#define MAX_FRAMES 48

/* Laptops with two GPUs (NVIDIA Optimus, AMD switchable graphics) run a program on the integrated one unless the
 * program exports these, the drivers' documented way to ask for the discrete GPU. A choice made for the program in
 * the driver's control panel or in Windows' graphics settings still wins. */
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

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

/* addr2line's output, or NULL when there is none on PATH. Run directly (not through cmd.exe, as _popen would), with
 * no window: rush2049pc.exe is a GUI program, and a console program it starts without a console of its own to share
 * would get a new console window. *proc is the process, for close_tool. */
static FILE *run_tool(char *cmd, HANDLE *proc)
{
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE r, w, nul;
    *proc = NULL;
    if (!CreatePipe(&r, &w, &sa, 0))
        return NULL;
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0,
                      NULL);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul, si.hStdOutput = w, si.hStdError = nul;
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(w);
    if (nul != INVALID_HANDLE_VALUE)
        CloseHandle(nul);
    if (!ok) {
        CloseHandle(r);
        return NULL;
    }
    CloseHandle(pi.hThread);
    *proc = pi.hProcess;
    int fd = _open_osfhandle((intptr_t)r, _O_RDONLY | _O_TEXT);
    FILE *f = fd >= 0 ? _fdopen(fd, "r") : NULL;
    if (!f) {
        fd >= 0 ? _close(fd) : CloseHandle(r);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        *proc = NULL;
    }
    return f;
}

static void close_tool(FILE *f, HANDLE proc)
{
    fclose(f);
    WaitForSingleObject(proc, 5000);
    CloseHandle(proc);
}

/* fault_first: pcs[0] is the faulting instruction itself, looked up at its own address; the others are return
 * addresses, looked up 1 byte before (inside the call), as a fault on a function's first instruction would otherwise
 * be put in the function before. */
static void print_frames(void **pcs, int n, int fault_first)
{
    char exe[MAX_PATH], name[MAX_PATH], cmd[8192], line[512];
    HMODULE self = GetModuleHandleW(NULL), mod[MAX_FRAMES];
    uintptr_t off[MAX_FRAMES];
    int len, i;
    FILE *p = NULL;
    HANDLE proc = NULL;
    if (n > MAX_FRAMES)
        n = MAX_FRAMES;
    for (i = 0; i < n; i++)
        mod[i] = module_of(pcs[i] ? (const uint8_t *)pcs[i] - (i == 0 && fault_first ? 0 : 1) : NULL, &off[i]);
    if (GetModuleFileNameA(self, exe, sizeof exe)) {
        len = snprintf(cmd, sizeof cmd, "addr2line -f -p -e \"%s\" -j .text", exe);
        for (i = 0; i < n && len < (int)sizeof cmd - 24; i++)
            if (mod[i] == self && off[i] != (uintptr_t)-1)
                len += snprintf(cmd + len, sizeof cmd - (size_t)len, " 0x%llx", (unsigned long long)off[i]);
        p = run_tool(cmd, &proc);
    }
    /* The frames are put together and written at once: stderr is unbuffered (log.c), so line by line other
     * threads' lines could land between them. */
    static char out[MAX_FRAMES * 600];
    size_t at = 0;
    for (i = 0; i < n && at < sizeof out; i++) {
        if (mod[i] == self && off[i] != (uintptr_t)-1 && p && fgets(line, sizeof line, p) && strncmp(line, "??", 2))
            at += (size_t)snprintf(out + at, sizeof out - at, "  #%-2d %s", i + 1, line);
        else if (mod[i] && GetModuleFileNameA(mod[i], name, sizeof name)) {
            const char *base = strrchr(name, '\\');
            at += (size_t)snprintf(out + at, sizeof out - at, "  #%-2d %s+0x%llx\n", i + 1, base ? base + 1 : name,
                                   (unsigned long long)((uintptr_t)pcs[i] - (uintptr_t)mod[i]));
        } else
            at += (size_t)snprintf(out + at, sizeof out - at, "  #%-2d %p\n", i + 1, pcs[i]);
    }
    if (at > sizeof out - 1)
        at = sizeof out - 1;
    fwrite(out, 1, at, stderr);
    if (p)
        close_tool(p, proc);
}

void pc_print_backtrace(int skip)
{
    CONTEXT ctx;
    void *pcs[MAX_FRAMES + 8];
    RtlCaptureContext(&ctx);
    int n = unwind(&ctx, pcs, MAX_FRAMES + skip + 1);
    if (n > skip + 1)
        print_frames(pcs + skip + 1, n - skip - 1, 0);
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
    print_frames(pcs, frames, 1);
    pc_si_fault_flush();   /* the saves, if that is safe (si.c), before the log is closed */
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

static BOOL WINAPI console_event(DWORD event)
{
    if (event != CTRL_CLOSE_EVENT && event != CTRL_LOGOFF_EVENT && event != CTRL_SHUTDOWN_EVENT)
        return FALSE;   /* Ctrl-C, Ctrl-Break: the C runtime's SIGINT and SIGBREAK */
    if (!pc_quit)
        pc_quit = 1 + 130;
    for (int i = 0; i < 300 && !pc_si_finished(); i++)
        Sleep(10);
    pc_si_finish();   /* at once if the loop's end has done it */
    Sleep(1000);      /* main's _exit, with the log emptied, ends the process meanwhile */
    return TRUE;
}

void pc_fault_install(void)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(fault);
    SetConsoleCtrlHandler(console_event, TRUE);
    pc_fault_thread_init();
}
