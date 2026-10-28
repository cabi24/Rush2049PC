/* port/app/log.c: the run's output in a log file as well as on the console (docs/port/RELEASE.md, "The log").
 *
 * stderr (fd 2) becomes a pipe, and a thread copies what comes out of it to the console (fd 2 as it was) and to
 * the file. Everything the program and its libraries print reaches both, the fault reports included: port/ultra's
 * fault handlers call pc_on_fault after the report, which empties the pipe into the file before the process ends.
 * The file is rush2049pc.log where the program keeps what it writes (port/app/paths.c), and the run before is
 * kept as rush2049pc.old.log. On Windows, a program that owns its console (a double-click: the window closes with
 * the program) waits for a key before it ends after an error or a fault, and a fault shows a message box saying
 * where the log is. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <io.h>
#include <windows.h>
#endif

#include "app.h"
#include "pc_compat.h"
#include "ultra.h"

#ifdef _WIN32
#define p_read _read
#define p_write(fd, b, n) _write(fd, b, (unsigned)(n))
#define p_dup _dup
#define p_dup2 _dup2
#define p_close _close
#else
#define p_read read
#define p_write write
#define p_dup dup
#define p_dup2 dup2
#define p_close close
#endif

extern const char pc_version[], pc_version_commit[], pc_version_build[];   /* cmake/version.cmake */

static struct {
    int active;               /* stderr goes through the pipe */
    int log_fd, console_fd, pipe_r;
    volatile int done;        /* the copying thread saw the pipe's end */
    int window;               /* the program has (or would have) a window: a fault shows a box */
    char path[PC_APP_PATH];
} L = {0, -1, -1, -1, 0, 0, ""};

/* ---------------------------------------------------------------- the version */

#if defined(_WIN32)
#define OS "Windows"
#elif defined(__linux__)
#define OS "Linux"
#else
#define OS "unknown OS"
#endif
#if defined(__x86_64__) || defined(_M_X64)
#define ARCH "x86-64"
#elif defined(__aarch64__)
#define ARCH "aarch64"
#else
#define ARCH "unknown CPU"
#endif
#if defined(__clang__)
#define COMPILER "clang " __clang_version__
#elif defined(__GNUC__)
#define COMPILER "gcc " __VERSION__
#else
#define COMPILER "unknown compiler"
#endif

#ifndef PC_BUILD_TYPE   /* CMakeLists.txt: $<CONFIG> */
#define PC_BUILD_TYPE ""
#endif

const char *pc_app_version(void)
{
    return pc_version;
}

const char *pc_app_version_commit(void)
{
    return pc_version_commit;
}

const char *pc_app_version_build(void)
{
    return pc_version_build;
}

const char *pc_app_build_type(void)
{
    return PC_BUILD_TYPE[0] ? PC_BUILD_TYPE : "unknown";
}

void pc_app_version_text(char *out, size_t size)
{
    snprintf(out, size, "rush2049pc %s\ncommit %s, %s, %s\n%s %s, %s\n", pc_version, pc_version_commit,
             pc_version_build, pc_app_build_type(), OS, ARCH, COMPILER);
}

/* ---------------------------------------------------------------- the file */

static void write_all(int fd, const char *p, size_t n)
{
    while (n > 0) {
        long k = (long)p_write(fd, p, n);
        if (k < 0 && errno == EINTR)
            continue;
        if (k <= 0)
            return;
        p += k, n -= (size_t)k;
    }
}

static void *copy(void *arg)
{
    char buf[4096];
    (void)arg;
    for (;;) {
        long n = (long)p_read(L.pipe_r, buf, sizeof buf);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        if (L.console_fd >= 0)
            write_all(L.console_fd, buf, (size_t)n);
        if (L.log_fd >= 0)
            write_all(L.log_fd, buf, (size_t)n);
    }
    L.done = 1;
    return NULL;
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

/* The old log's name: rush2049pc.log -> rush2049pc.old.log; another name gets .old. */
static void old_name(const char *path, char *out, size_t size)
{
    size_t n = strlen(path);
    if (n > 4 && !strcmp(path + n - 4, ".log"))
        snprintf(out, size, "%.*s.old.log", (int)(n - 4), path);
    else
        snprintf(out, size, "%s.old", path);
}

static void on_abort(int sig)
{
    static const char msg[] = "\nrush2049pc: aborted\n";
    write_all(2, msg, sizeof msg - 1);
    pc_print_backtrace(1);
    pc_app_on_fault("the program aborted");
    signal(sig, SIG_DFL);
    raise(sig);
}

int pc_app_log_open(const char *path, int argc, char **argv, const PcAppDirs *d)
{
    char dir[PC_APP_PATH], old[PC_APP_PATH + 8], text[2048];
    if (L.active)
        return 0;
    snprintf(L.path, sizeof L.path, "%s", path);
    pc_app_dirname(path, dir, sizeof dir);
    if (pc_app_mkdirs(dir) != 0) {
        fprintf(stderr, "rush2049pc: no log: cannot make the folder %s: %s\n", dir, strerror(errno));
        return -1;
    }
    old_name(path, old, sizeof old);
    if (pc_app_exists(path))
        pc_rename_replace(path, old);
#ifdef _WIN32
    L.log_fd = _open(path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
#else
    L.log_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
#endif
    if (L.log_fd < 0) {
        fprintf(stderr, "rush2049pc: no log: cannot write %s: %s\n", path, strerror(errno));
        return -1;
    }

    /* What the log starts with: the version (to find the matching build for a backtrace), when, where, how. */
    time_t now = time(NULL);
    struct tm tm;
#ifdef _WIN32
    tm = *localtime(&now);
#else
    localtime_r(&now, &tm);
#endif
    char when[64];
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &tm);
    pc_app_version_text(text, sizeof text);
    size_t n = strlen(text);
    n += (size_t)snprintf(text + n, sizeof text - n, "started %s; program folder %s; working directory %s; %s\n",
                          when, d->exe[0] ? d->exe : "(unknown)", d->cwd[0] ? d->cwd : "(unknown)",
                          d->portable ? "portable (rush2049pc.ini next to the program)" : "installed");
    n += (size_t)snprintf(text + n, sizeof text - n, "command line:");
    for (int i = 0; i < argc && n < sizeof text - 8; i++)
        n += (size_t)snprintf(text + n, sizeof text - n, strchr(argv[i], ' ') ? " \"%s\"" : " %s", argv[i]);
    if (n < sizeof text - 2)
        n += (size_t)snprintf(text + n, sizeof text - n, "\n\n");
    write_all(L.log_fd, text, n < sizeof text ? n : sizeof text - 1);

    /* stderr through a pipe */
    int fds[2];
    fflush(stderr);
#ifdef _WIN32
    if (_pipe(fds, 1 << 16, _O_BINARY | _O_NOINHERIT) != 0)
#else
    if (pipe2(fds, O_CLOEXEC) != 0)
#endif
    {
        fprintf(stderr, "rush2049pc: the log has only its header: no pipe: %s\n", strerror(errno));
        return -1;
    }
    L.pipe_r = fds[0];
    L.console_fd = p_dup(2);   /* -1 when there is no stderr: the file alone */
    if (p_dup2(fds[1], 2) < 0) {
        p_close(fds[0]), p_close(fds[1]);
        fprintf(stderr, "rush2049pc: the log has only its header: %s\n", strerror(errno));
        return -1;
    }
    p_close(fds[1]);   /* fd 2 is the only writing end, so restoring it ends the pipe */
#ifdef _WIN32
    SetStdHandle(STD_ERROR_HANDLE, (HANDLE)_get_osfhandle(2));   /* for WriteFile(GetStdHandle(...)) */
#else
    setvbuf(stderr, NULL, _IOLBF, 0);
#endif
    pthread_t t;
    if (pthread_create(&t, NULL, copy, NULL) != 0) {
        p_dup2(L.console_fd, 2);
        fprintf(stderr, "rush2049pc: the log has only its header: no thread\n");
        return -1;
    }
    pthread_detach(t);
    L.active = 1;
    signal(SIGABRT, on_abort);
    return 0;
}

/* Puts stderr back and waits (up to 3 s) until everything that went into the pipe is in the file. Only calls that
 * are safe in a signal handler, since a fault handler calls it. */
void pc_app_log_close(void)
{
    if (!L.active)
        return;
    L.active = 0;
    fflush(stderr);
    if (L.console_fd >= 0)
        p_dup2(L.console_fd, 2);
    else
        p_close(2);
#ifdef _WIN32
    SetStdHandle(STD_ERROR_HANDLE, (HANDLE)_get_osfhandle(2));
#endif
    for (int i = 0; i < 3000 && !L.done; i++)
        nap_ms(1);
#ifndef _WIN32
    fsync(L.log_fd);
#endif
}

const char *pc_app_log_path(void)
{
    return L.log_fd >= 0 ? L.path : NULL;
}

void pc_app_log_window(int window)
{
    L.window = window;
}

/* ---------------------------------------------------------------- the end */

#ifdef _WIN32
/* A double-clicked program has a console of its own, which closes when it ends: then (or with RUSH2049_WAIT_KEY=1)
 * it waits for a key, so the last messages can be read. RUSH2049_WAIT_KEY=0 never waits. */
static void wait_for_key(void)
{
    const char *e = getenv("RUSH2049_WAIT_KEY");
    DWORD list[2], mode;
    if (e && !strcmp(e, "0"))
        return;
    int forced = e && !strcmp(e, "1");
    if (!forced && GetConsoleProcessList(list, 2) != 1)
        return;
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    fprintf(stderr, "\nrush2049pc: press a key to close this window.\n");
    fflush(stderr);
    if (GetConsoleMode(in, &mode)) {
        FlushConsoleInputBuffer(in);
        _getch();
    } else if (forced) {
        getchar();
    }
}
#endif

void pc_app_on_fault(const char *what)
{
    char text[3 * PC_APP_PATH];
    const char *log = pc_app_log_path();
    pc_app_log_close();
    snprintf(text, sizeof text, "The program stopped because of an error:\n    %s\n\n%s%s%s"
             "Please report it at https://github.com/cabi24/Rush2049PC/issues%s.", what,
             log ? "The log, with the details, is\n    " : "", log ? log : "", log ? "\n\n" : "",
             log ? ", with the log" : "");
    if (log)
        fprintf(stderr, "rush2049pc: the log is %s\n", log);
#ifdef _WIN32
    pc_app_alert_native(L.window, "Rush 2049 PC stopped", text);
    wait_for_key();
#endif
}

void pc_app_finish(int code)
{
    pc_app_log_close();
#ifdef _WIN32
    if (code != 0)
        wait_for_key();
#else
    (void)code;
#endif
}
