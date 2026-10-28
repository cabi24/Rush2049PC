/* port/app/pick.c: the "Open ROM" file dialog of the first run (docs/port/RELEASE.md, "Choosing the ROM").
 *
 * Windows: the system's own dialog, IFileOpenDialog (ole32, which the program already imports through SDL2: no new
 * DLL). Linux: zenity (GNOME, most desktops) or kdialog (KDE first when it is the desktop), run with posix_spawn and
 * read from their standard output; with neither, there is no dialog and the ROM's message box says what to do, as
 * before. xdg-desktop-portal's FileChooser is not used: its answer comes as a D-Bus signal, which needs a D-Bus
 * library the program doesn't link.
 *
 * RUSH2049_TEST_ROM_PICK="ANSWER|ANSWER|..." stands in for the player (tools/port/firstrun_check.py): each dialog
 * takes the next answer, a path or :cancel (the player cancelled) or :fail (no dialog could be shown), and alert.c
 * prints every box and answers its question with the first button while answers are left, else the last. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#endif

#include "app.h"

#define TITLE "Open your San Francisco Rush 2049 (USA) ROM"

/* ---------------------------------------------------------------- the test hook */

static struct {
    int parsed, set, count, next;
    char *answers[16];
} T;

static void test_parse(void)
{
    if (T.parsed)
        return;
    T.parsed = 1;
    const char *e = getenv("RUSH2049_TEST_ROM_PICK");
    if (!e)
        return;
    T.set = 1;
    char *s = strdup(e);
    for (char *p = s; s && *p && T.count < 16;) {
        char *bar = strchr(p, '|');
        if (bar)
            *bar = 0;
        T.answers[T.count++] = p;
        if (!bar)
            break;
        p = bar + 1;
    }
}

int pc_app_pick_test(void)
{
    test_parse();
    return T.set;
}

int pc_app_pick_pending(void)
{
    test_parse();
    return T.set && T.next < T.count;
}

static int test_pick(char *out, size_t size)
{
    if (T.next >= T.count)
        return 1;
    const char *a = T.answers[T.next++];
    fprintf(stderr, "rush2049pc: file dialog (RUSH2049_TEST_ROM_PICK): %s\n", a);
    if (!strcmp(a, ":cancel"))
        return 1;
    if (!strcmp(a, ":fail"))
        return -1;
    snprintf(out, size, "%s", a);
    return 0;
}

/* ---------------------------------------------------------------- Windows */

#ifdef _WIN32
/* Their own copies, so that no uuid library is needed (shobjidl.h's values). */
static const CLSID file_open_dialog = {0xDC1C5A9C, 0xE88A, 0x4DDE, {0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7}};
static const IID file_open_dialog_iid = {0xD57C7288, 0xD4AD, 0x4768, {0xBE, 0x02, 0x9D, 0x96, 0x95, 0x32, 0xD9, 0x60}};

/* The path in the code page the C runtime's fopen takes (the program's paths are char strings). A name it can't
 * spell is tried as its short (8.3) name. 0, or -1 with a message in out. */
static int narrow(const wchar_t *wide, char *out, size_t size)
{
    BOOL lossy = FALSE;
    int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide, -1, out, (int)size, NULL, &lossy);
    if (n > 0 && !lossy)
        return 0;
    wchar_t brief[MAX_PATH];
    DWORD k = GetShortPathNameW(wide, brief, MAX_PATH);
    lossy = FALSE;
    if (k > 0 && k < MAX_PATH &&
        WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, brief, -1, out, (int)size, NULL, &lossy) > 0 && !lossy)
        return 0;
    snprintf(out, size, "its folder or name has letters this program can't open yet: copy the ROM to a folder "
                        "such as C:\\Games and choose it there");
    return -1;
}

static int windows_pick(char *out, size_t size)
{
    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog *dlg = NULL;
    int rc = -1;
    if (SUCCEEDED(CoCreateInstance(&file_open_dialog, NULL, CLSCTX_INPROC_SERVER, &file_open_dialog_iid,
                                   (void **)&dlg))) {
        COMDLG_FILTERSPEC types[] = {{L"N64 ROMs (*.z64, *.v64, *.n64)", L"*.z64;*.v64;*.n64"},
                                     {L"All files", L"*.*"}};
        FILEOPENDIALOGOPTIONS opts = 0;
        IFileOpenDialog_SetFileTypes(dlg, 2, types);
        IFileOpenDialog_SetTitle(dlg, L"" TITLE);
        IFileOpenDialog_SetOkButtonLabel(dlg, L"Open ROM");
        if (SUCCEEDED(IFileOpenDialog_GetOptions(dlg, &opts)))
            IFileOpenDialog_SetOptions(dlg, opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST |
                                                FOS_NOCHANGEDIR | FOS_DONTADDTORECENT);
        HRESULT hr = IFileOpenDialog_Show(dlg, NULL);
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            rc = 1;
        } else if (SUCCEEDED(hr)) {
            IShellItem *item = NULL;
            PWSTR wide = NULL;
            if (SUCCEEDED(IFileOpenDialog_GetResult(dlg, &item)) &&
                SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &wide))) {
                rc = narrow(wide, out, size) == 0 ? 0 : -2;
                CoTaskMemFree(wide);
            }
            if (item)
                IShellItem_Release(item);
        }
        IFileOpenDialog_Release(dlg);
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    return rc;
}
#endif

/* ---------------------------------------------------------------- Linux */

#ifndef _WIN32
extern char **environ;

static int find_program(const char *name, char *out, size_t size)
{
    const char *path = getenv("PATH");
    if (!path || !*path)
        path = "/usr/local/bin:/usr/bin:/bin";
    for (const char *p = path;;) {
        const char *colon = strchr(p, ':');
        size_t n = colon ? (size_t)(colon - p) : strlen(p);
        if (n && *p == '/') {
            snprintf(out, size, "%.*s/%s", (int)n, p, name);
            if (access(out, X_OK) == 0)
                return 0;
        }
        if (!colon)
            return -1;
        p = colon + 1;
    }
}

/* The dialog program: 0 with the path it printed, 1 when it says the player cancelled (exit 1), -1 otherwise. */
static int run_dialog(const char *exe, char *const argv[], char *out, size_t size)
{
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0)
        return -1;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 1);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);   /* GTK's warnings stay out of the log */
    pid_t pid;
    int err = posix_spawn(&pid, exe, &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fds[1]);
    if (err) {
        close(fds[0]);
        return -1;
    }
    size_t got = 0;
    int too_long = 0;
    for (;;) {
        char scratch[256];
        char *at = got < size - 1 ? out + got : scratch;
        size_t room = got < size - 1 ? size - 1 - got : sizeof scratch;
        ssize_t k = read(fds[0], at, room);
        if (k < 0 && errno == EINTR)
            continue;
        if (k <= 0)
            break;
        if (at == scratch)
            too_long = 1;
        else
            got += (size_t)k;
    }
    close(fds[0]);
    out[got] = 0;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
    while (got && (out[got - 1] == '\n' || out[got - 1] == '\r'))
        out[--got] = 0;
    if (!WIFEXITED(status))
        return -1;
    if (WEXITSTATUS(status) == 1)
        return 1;
    if (WEXITSTATUS(status) != 0 || !got || too_long || strchr(out, '\n'))
        return -1;
    return 0;
}

static int linux_pick(char *out, size_t size)
{
    const char *desktop = getenv("XDG_CURRENT_DESKTOP"), *home = getenv("HOME");
    int kde_first = desktop && strstr(desktop, "KDE");
    char exe[1024], start[1024];
    snprintf(start, sizeof start, "%s/", home && *home ? home : ".");
    for (int i = 0; i < 2; i++) {
        int kde = (i == 0) == kde_first;
        if (find_program(kde ? "kdialog" : "zenity", exe, sizeof exe) != 0)
            continue;
        char *zenity[] = {exe, "--file-selection", "--title=" TITLE, "--filename", start,
                          "--file-filter=N64 ROMs (.z64, .v64, .n64) | *.z64 *.v64 *.n64 *.Z64 *.V64 *.N64",
                          "--file-filter=All files | *", NULL};
        char *kdialog[] = {exe, "--title", TITLE, "--getopenfilename", start,
                           "N64 ROMs (*.z64 *.v64 *.n64 *.Z64 *.V64 *.N64)", NULL};
        int rc = run_dialog(exe, kde ? kdialog : zenity, out, size);
        if (rc >= 0)
            return rc;
        fprintf(stderr, "rush2049pc: %s gave no file\n", exe);
    }
    return -1;
}
#endif

/* ---------------------------------------------------------------- the dialog */

int pc_app_pick_available(void)
{
    if (pc_app_pick_test())
        return 1;
#ifdef _WIN32
    return 1;
#else
    char exe[1024];
    return find_program("zenity", exe, sizeof exe) == 0 || find_program("kdialog", exe, sizeof exe) == 0;
#endif
}

int pc_app_pick_rom(char *out, size_t size)
{
    out[0] = 0;
    if (pc_app_pick_test())
        return test_pick(out, size);
#ifdef _WIN32
    int rc = windows_pick(out, size);
    if (rc == -2) {   /* a path the program can't open: said by the caller as the file's problem */
        fprintf(stderr, "rush2049pc: the file chosen: %s\n", out);
        return 2;
    }
    return rc;
#else
    return linux_pick(out, size);
#endif
}
