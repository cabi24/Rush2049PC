/* port/app/startup.c: what main does before anything else (docs/port/WINDOWS.md, "The console"; RELEASE.md, "The
 * log").
 *
 * Windows: rush2049pc.exe is a GUI program (-mwindows), so Windows never opens a console for it. Its standard
 * handles are what its starter gave it: files or pipes when the output is redirected (a script, CI, a launcher),
 * nothing when it is double-clicked or started from cmd.exe or PowerShell. With nothing, the program attaches to
 * its parent's console (AttachConsole(ATTACH_PARENT_PROCESS)) and opens its standard streams on it, so
 * `rush2049pc.exe --help` prints in the prompt's window. Then it sets the process's timer period and power
 * throttling for the whole run.
 *
 * Without a console to attach to (a double-click, a shortcut) nobody sees what the program prints. Then stdout and
 * stderr go into a pipe whose reader keeps the last few KiB ("quiet"): the log still gets everything (log.c copies
 * stderr to the file and to what fd 2 was, this pipe), and when the program ends with an error that no message box
 * has explained yet, pc_app_finish shows a box with the last lines printed. RUSH2049_CONSOLE=none makes any run
 * quiet, on Linux too, for the tests; RUSH2049_MESSAGE_BOX=print then prints the box to the real stderr.
 *
 * The program again at a run's end (pc_app_restart, app.h): the F1 Online page's restarts and a guest's rebuild after
 * a desync, one way for both, on the console's terms above: exec on Linux; on Windows a new process that inherits
 * this one's standard handles and is waited for when they are seen (a console, a file, a pipe), or runs on its own
 * when nobody sees them (quiet). */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#include <mmsystem.h>   /* timeBeginPeriod */
#else
#include <sys/ioctl.h>
#endif

#include "app.h"
#include "pc_online.h"   /* the restart's command line, quoted for Windows */

#define RING 4096

static struct {
    int quiet;       /* nobody sees the output: it goes into the ring */
    int attached;    /* Windows: attached to the parent's console */
    int box_shown;   /* a message box has said what went wrong */
    int print_fd;    /* where RUSH2049_MESSAGE_BOX=print writes */
    int pipe_r;
    pthread_mutex_t lock;
    char ring[RING];
    size_t total;    /* bytes ever put in the ring */
} C = {0, 0, 0, 2, -1, PTHREAD_MUTEX_INITIALIZER, "", 0};

/* ---------------------------------------------------------------- the ring of what was printed */

static long pipe_available(int fd)
{
#ifdef _WIN32
    DWORD n = 0;
    if (!PeekNamedPipe((HANDLE)_get_osfhandle(fd), NULL, 0, NULL, &n, NULL))
        return -1;
    return (long)n;
#else
    int n = 0;
    if (ioctl(fd, FIONREAD, &n) != 0)
        return -1;
    return n;
#endif
}

/* With C.lock held: what the pipe holds into the ring, without blocking. -1 when the pipe has ended. */
static int take(void)
{
    char buf[1024];
    long n;
    while ((n = pipe_available(C.pipe_r)) > 0) {
        long k = (long)read(C.pipe_r, buf, (unsigned)(n < (long)sizeof buf ? n : (long)sizeof buf));
        if (k <= 0)
            return -1;
        for (long i = 0; i < k; i++)
            C.ring[C.total++ % RING] = buf[i];
    }
    return n < 0 ? -1 : 0;
}

static void nap_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts = {0, ms * 1000000L};
    nanosleep(&ts, NULL);
#endif
}

/* Empties the pipe every 20 ms. It polls, rather than blocking in read, so that pc_app_console_tail can empty
 * the pipe itself under the lock and know that it has everything printed before. */
static void *reader(void *arg)
{
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&C.lock);
        int end = take();
        pthread_mutex_unlock(&C.lock);
        if (end < 0)
            return NULL;
        nap_ms(20);
    }
}

void pc_app_console_tail(char *out, size_t size)
{
    char text[RING + 1];
    size_t n, start;
    out[0] = 0;
    if (!C.quiet || size == 0)
        return;
    fflush(stdout);
    fflush(stderr);
    pthread_mutex_lock(&C.lock);
    take();
    n = C.total < RING ? C.total : RING;
    for (size_t i = 0; i < n; i++)
        text[i] = C.ring[(C.total - n + i) % RING];
    pthread_mutex_unlock(&C.lock);
    text[n] = 0;
    for (size_t i = 0, j = 0; i <= n; i++)   /* no CRs */
        if (text[i] != '\r')
            text[j++] = text[i];
    n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == ' '))
        text[--n] = 0;
    /* The last 12 lines, and no more than fits */
    start = n;
    for (int lines = 0; start > 0 && lines < 12; start--)
        if (text[start - 1] == '\n' && ++lines == 12)
            break;
    if (n - start > size - 1)
        start = n - (size - 1);
    snprintf(out, size, "%s", text + start);
}

int pc_app_console_quiet(void)
{
    return C.quiet;
}

int pc_app_console_print_fd(void)
{
    return C.print_fd;
}

void pc_app_box_shown(void)
{
    C.box_shown = 1;
}

int pc_app_box_was_shown(void)
{
    return C.box_shown;
}

/* stdout and stderr into the ring's pipe (only those of fds 1 and 2 that nobody sees: which[fd]). */
static int capture(const int which[3])
{
    int fds[2];
#ifdef _WIN32
    if (_pipe(fds, 1 << 16, _O_BINARY | _O_NOINHERIT) != 0)
        return -1;
#else
    if (pipe2(fds, O_CLOEXEC) != 0)
        return -1;
#endif
    fflush(stdout);
    fflush(stderr);
    for (int fd = 1; fd <= 2; fd++)
        if (which[fd])
            dup2(fds[1], fd);
    close(fds[1]);
    C.pipe_r = fds[0];
    pthread_t t;
    if (pthread_create(&t, NULL, reader, NULL) != 0) {
        close(fds[0]);
        C.pipe_r = -1;
        return -1;
    }
    pthread_detach(t);
#ifdef _WIN32
    SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)_get_osfhandle(1));
    SetStdHandle(STD_ERROR_HANDLE, (HANDLE)_get_osfhandle(2));
#endif
    return 0;
}

/* ---------------------------------------------------------------- Windows: the console */

#ifdef _WIN32
enum { NONE, REDIRECTED, CONSOLE, OTHER_CHAR };

/* What the C runtime's fd is: nothing (a GUI program started without handles: msvcrt keeps -1 there, the UCRT
 * -2), a file or a pipe, a console this process can use, or another character device: NUL, or the handle of a
 * console this process isn't attached to. */
static int fd_kind(int fd)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    DWORD mode;
    if (h == INVALID_HANDLE_VALUE || h == NULL || h == (HANDLE)(intptr_t)-2)
        return NONE;
    switch (GetFileType(h) & ~FILE_TYPE_REMOTE) {
    case FILE_TYPE_DISK:
    case FILE_TYPE_PIPE:
        return REDIRECTED;
    case FILE_TYPE_CHAR:
        return GetConsoleMode(h, &mode) ? CONSOLE : OTHER_CHAR;
    default:
        return NONE;
    }
}

/* fd (and its stream) opened on name: CONIN$, CONOUT$ or NUL. The slot is closed first, so that freopen's new
 * descriptor is fd itself, the lowest free one (0 to fd - 1 are open: they are done in order). That also gives
 * the stream its descriptor back where the UCRT had set it to -2. */
static void reopen(int fd, const char *name)
{
    FILE *f = fd == 0 ? stdin : fd == 1 ? stdout : stderr;
    _close(fd);
    if (!freopen(name, fd == 0 ? "r" : "w", f))
        return;
    if (_fileno(f) != fd)   /* not expected; the descriptor at least */
        _dup2(_fileno(f), fd);
    if (fd == 2)
        setvbuf(f, NULL, _IONBF, 0);
}

static void console_init(int force_quiet)
{
    int kind[3], gone[3] = {0, 0, 0};
    for (int fd = 0; fd < 3; fd++)
        kind[fd] = fd_kind(fd);
    /* The parent's console, when stdout or stderr has no usable handle: from cmd.exe or PowerShell, whose
     * consoles a GUI program isn't attached to; from Explorer, which has none, it fails. */
    if (!force_quiet && (kind[1] == NONE || kind[1] == OTHER_CHAR || kind[2] == NONE || kind[2] == OTHER_CHAR) &&
        AttachConsole(ATTACH_PARENT_PROCESS))
        C.attached = 1;
    for (int fd = 0; fd < 3; fd++) {
        DWORD std = fd == 0 ? STD_INPUT_HANDLE : fd == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE, mode;
        if (C.attached && (kind[fd] == NONE || kind[fd] == OTHER_CHAR)) {
            /* A handle that works now that the console is ours stays; so does NUL that the parent gave (2>nul):
             * attaching left that standard handle alone. Otherwise the console. */
            HANDLE now = GetStdHandle(std), mine = (HANDLE)_get_osfhandle(fd);
            if (kind[fd] == OTHER_CHAR && (GetConsoleMode(mine, &mode) || now == mine))
                continue;
            reopen(fd, fd == 0 ? "CONIN$" : "CONOUT$");
        } else if (kind[fd] == NONE || (force_quiet && fd > 0 && kind[fd] != REDIRECTED)) {
            reopen(fd, "NUL");   /* valid, for the capture's dup2 or for reading nothing */
            gone[fd] = 1;
        }
    }
    for (int fd = 0; fd < 3; fd++)
        SetStdHandle(fd == 0 ? STD_INPUT_HANDLE : fd == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE,
                     (HANDLE)_get_osfhandle(fd));
    /* Nobody sees stderr: no console, and it isn't a file or a pipe. */
    if (force_quiet || (!C.attached && gone[2])) {
        int which[3] = {0, gone[1] || force_quiet, 1};
        if (force_quiet && kind[2] == REDIRECTED)
            C.print_fd = _dup(2);   /* the test reads the box here */
        if (capture(which) == 0)
            C.quiet = 1;
    }
}

/* For the whole run: a 1 ms timer period (Windows' default is 15.6 ms; the scheduler's waits don't depend on it,
 * docs/port/WINDOWS.md 5, but the CRT's and SDL's sleeps do), and no power throttling. Windows 11 may ignore the
 * period of a process that has no visible window (a headless run; the game minimized) and may run a background
 * process at its efficiency level (EcoQoS); both are turned off with SetProcessInformation (Windows 8 and later,
 * looked up; a Windows that doesn't know one of the two flags refuses the pair, so execution speed alone is tried
 * next). */
static void process_settings(void)
{
    typedef BOOL(WINAPI * SetInfo)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    SetInfo set = (SetInfo)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation");
    PROCESS_POWER_THROTTLING_STATE s = {PROCESS_POWER_THROTTLING_CURRENT_VERSION,
                                        PROCESS_POWER_THROTTLING_EXECUTION_SPEED |
                                            PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,
                                        0};
    if (set && !set(GetCurrentProcess(), ProcessPowerThrottling, &s, sizeof s)) {
        s.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        set(GetCurrentProcess(), ProcessPowerThrottling, &s, sizeof s);
    }
    timeBeginPeriod(1);   /* ended by the process's end */
}
#endif

/* ---------------------------------------------------------------- the program again */

/* The environment the program was started with: pc_app_startup keeps it before main sets anything (main sets
 * RUSH_GFX_* and the others from the settings in effect, for port/gfx, and a new run would take those as the
 * environment's layer over its settings file). Windows: a copy of the wide block, so a variable with letters outside
 * the code page (APPDATA under such a user name) reaches the new run as it was. */
#ifdef _WIN32
static wchar_t *s_start_env;
#else
extern char **environ;
static char **s_start_env;
#endif

static void keep_environment(void)
{
#ifdef _WIN32
    wchar_t *e = GetEnvironmentStringsW(), *p = e;
    size_t n;
    if (!e)
        return;
    while (*p)
        p += wcslen(p) + 1;
    n = (size_t)(p - e) + 1;
    if ((s_start_env = malloc(n * sizeof *e)) != NULL)
        memcpy(s_start_env, e, n * sizeof *e);
    FreeEnvironmentStringsW(e);
#else
    int n = 0;
    while (environ && environ[n])
        n++;
    if (!(s_start_env = calloc((size_t)n + 1, sizeof *s_start_env)))
        return;
    for (int i = 0; i < n; i++)
        s_start_env[i] = strdup(environ[i]);
#endif
}

static int restart_failed(const char *why, const char *cmd)
{
    char text[600];
    snprintf(text, sizeof text, "The program couldn't start again (%s). Start it again yourself%s%.400s", why,
             cmd ? ": " : ".", cmd ? cmd : "");
    fprintf(stderr, "rush2049pc: %s\n", text);
    pc_app_alert(1, "Rush 2049 PC", text);
    return 1;
}

int pc_app_restart(const char *const *argv, int argc, int flags)
{
    fflush(NULL);
#ifdef _WIN32
    static char cmd[32768];
    static wchar_t wcmd[32768];
    wchar_t exe[PC_APP_PATH];
    char exe_a[PC_APP_PATH], why[64];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD got = 1, n = GetModuleFileNameW(NULL, exe, PC_APP_PATH);
    int wait = !C.quiet;   /* a console, a file or a pipe saw its output: as exec, the same handles, waited for */
    if (n == 0 || n >= PC_APP_PATH)
        return restart_failed("its own path is too long", NULL);
    if (flags & PC_APP_RESTART_SAME) {   /* as Windows gave it: every character, the program's own name first */
        const wchar_t *line = GetCommandLineW();
        if (wcslen(line) >= sizeof wcmd / sizeof *wcmd)
            return restart_failed("its command line is too long", NULL);
        wcscpy(wcmd, line);
        cmd[0] = 0;
    } else {   /* argv[0] the program's own path; quoted as the C runtime parses it back (port/config/online.c) */
        const char **v = malloc(((size_t)argc + 1) * sizeof *v);
        DWORD k = GetModuleFileNameA(NULL, exe_a, sizeof exe_a);
        int len;
        if (!v)
            return restart_failed("out of memory", NULL);
        for (int i = 0; i < argc; i++)
            v[i] = argv[i];
        if (k > 0 && k < sizeof exe_a)
            v[0] = exe_a;
        v[argc] = NULL;
        len = pc_online_quote_windows(v, argc, cmd, sizeof cmd);
        free(v);
        if (len < 0 || !MultiByteToWideChar(CP_ACP, 0, cmd, -1, wcmd, (int)(sizeof wcmd / sizeof *wcmd)))
            return restart_failed("its command line is too long", NULL);
    }
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (wait) {
        HANDLE h[3] = {GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)};
        for (int i = 0; i < 3; i++)   /* inheritable, so the new program has them (a console's, a file's, a pipe's) */
            if (h[i] && h[i] != INVALID_HANDLE_VALUE)
                SetHandleInformation(h[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = h[0], si.hStdOutput = h[1], si.hStdError = h[2];
    }
    wchar_t *env = (flags & PC_APP_RESTART_START_ENV) ? s_start_env : NULL;
    if (!CreateProcessW(exe, wcmd, NULL, NULL, wait, env ? CREATE_UNICODE_ENVIRONMENT : 0, env, NULL, &si, &pi)) {
        snprintf(why, sizeof why, "CreateProcess: error %lu", (unsigned long)GetLastError());
        return restart_failed(why, cmd[0] ? cmd : NULL);
    }
    CloseHandle(pi.hThread);
    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (!GetExitCodeProcess(pi.hProcess, &got))
            got = 1;
    } else {
        got = 0;   /* nobody sees either program's output: this one ends, the new one has its own */
    }
    CloseHandle(pi.hProcess);
    return (int)got;
#else
    char **env = (flags & PC_APP_RESTART_START_ENV) && s_start_env ? s_start_env : environ;
    (void)argc;
#ifdef __linux__
    execve("/proc/self/exe", (char *const *)argv, env);   /* this build, even when its file was replaced since */
#endif
    environ = env;
    execvp(argv[0], (char *const *)argv);   /* no /proc: argv[0] as it was started */
    return restart_failed(strerror(errno), NULL);
#endif
}

void pc_app_startup(void)
{
    keep_environment();   /* before anything sets a variable: the restart's (pc_app_restart) */
    const char *e = getenv("RUSH2049_CONSOLE");
    int force_quiet = e && !strcmp(e, "none");
#ifdef _WIN32
    /* The C runtime's own errors (abort, assert) as a console program has them, on stderr: a GUI program's would
     * be a message box, which would hold a test run until someone clicks it. */
    _set_error_mode(_OUT_TO_STDERR);
    console_init(force_quiet);
    process_settings();
#else
    if (force_quiet) {
        int which[3] = {0, 1, 1};
        C.print_fd = dup(2);
        if (capture(which) == 0)
            C.quiet = 1;
    }
#endif
}
