/* port/app/paths.c: the program's folders and the places the ROM is looked for (docs/port/RELEASE.md). Only
 * named places are checked, in a fixed order; no folder is ever searched. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "app.h"
#include "pc_compat.h"

void pc_app_join(char *out, size_t size, const char *dir, const char *name)
{
    size_t n = strlen(dir);
    int sep = n && dir[n - 1] != '/' && dir[n - 1] != PC_APP_SEP;
    if (!n)
        snprintf(out, size, "%s", name);
    else
        snprintf(out, size, "%s%s%s", dir, sep ? (PC_APP_SEP == '/' ? "/" : "\\") : "", name);
}

int pc_app_is_absolute(const char *p)
{
#ifdef _WIN32
    /* C:\x, C:/x, \\server\share, and \x (the current drive's root) */
    return p[0] == '\\' || p[0] == '/' || (p[0] && p[1] == ':' && (p[2] == '\\' || p[2] == '/'));
#else
    return p[0] == '/';
#endif
}

int pc_app_exists(const char *path)
{
    struct stat st;
    return path[0] && stat(path, &st) == 0;
}

void pc_app_dirname(const char *path, char *out, size_t size)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *back = strrchr(path, '\\');
    if (back && (!slash || back > slash))
        slash = back;
#endif
    if (!slash)
        snprintf(out, size, ".");
    else if (slash == path)
        snprintf(out, size, "%c", *path);
    else
        snprintf(out, size, "%.*s", (int)(slash - path), path);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int pc_app_mkdirs(const char *dir)
{
    char p[PC_APP_PATH];
    snprintf(p, sizeof p, "%s", dir);
    for (char *c = p + 1;; c++)
        if (*c == '/' || *c == PC_APP_SEP || !*c) {
            char sep = *c;
            *c = 0;
            /* a drive ("C:") or a folder that is there is skipped: making one fails with EACCES on Windows */
            if (c[-1] != ':' && !is_dir(p) && mkdir(p, 0755) != 0 && errno != EEXIST)
                return -1;
            if (!sep)
                return 0;
            *c = sep;
        }
}

static int is_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void user_dir(char *out, size_t n)
{
    out[0] = 0;
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata && *appdata)
        pc_app_join(out, n, appdata, "rush2049pc");
#else
    const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    char base[PC_APP_PATH];
    if (xdg && xdg[0] == '/') {   /* the XDG spec: a relative XDG_DATA_HOME is ignored */
        pc_app_join(out, n, xdg, "rush2049pc");
    } else if (home && *home) {
        pc_app_join(base, sizeof base, home, ".local/share");
        pc_app_join(out, n, base, "rush2049pc");
    }
#endif
}

void pc_app_dirs(PcAppDirs *d, const char *exe_dir)
{
    char ini[PC_APP_PATH];
    memset(d, 0, sizeof *d);
    snprintf(d->exe, sizeof d->exe, "%s", exe_dir ? exe_dir : "");
    user_dir(d->user, sizeof d->user);
    if (!getcwd(d->cwd, sizeof d->cwd))
        d->cwd[0] = 0;
    pc_app_join(ini, sizeof ini, d->exe, "rush2049pc.ini");
    d->portable = d->exe[0] && is_file(ini);
    snprintf(d->data, sizeof d->data, "%s", d->portable ? d->exe : d->user[0] ? d->user : d->cwd);
}

static int same_path(const char *a, const char *b)
{
#ifdef _WIN32
    return !strcasecmp(a, b);
#else
    return !strcmp(a, b);
#endif
}

int pc_app_rom_places(const PcAppDirs *d, const char *name, int from_command_line, PcAppPlace out[PC_APP_ROM_PLACES])
{
    int n = 0;
    if (!name || !*name)
        return 0;
    if (from_command_line || pc_app_is_absolute(name)) {
        snprintf(out[0].path, sizeof out[0].path, "%s", name);
        out[0].where = from_command_line ? "the path given" : "the path in the settings (game.rom)";
        return 1;
    }
    const struct { const char *dir, *where; } dirs[] = {
        {d->exe, "next to the program"}, {d->user, "the user data folder"}, {d->cwd, "the working directory"}};
    for (int i = 0; i < 3; i++) {
        char path[PC_APP_PATH];
        int seen = 0;
        if (!dirs[i].dir[0])
            continue;
        pc_app_join(path, sizeof path, dirs[i].dir, name);
        for (int j = 0; j < n && !seen; j++)
            seen = same_path(out[j].path, path);
        if (seen)
            continue;
        snprintf(out[n].path, sizeof out[n].path, "%s", path);
        out[n++].where = dirs[i].where;
    }
    return n;
}

int pc_app_rom_find(const PcAppPlace *places, int n)
{
    for (int i = 0; i < n; i++)
        if (pc_app_exists(places[i].path))
            return i;
    return -1;
}
