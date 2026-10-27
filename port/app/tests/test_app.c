/* port/app/tests/test_app.c: the program's folders and the ROM lookup order (port/app/paths.c,
 * docs/port/RELEASE.md), in temporary folders under the directory given: a program folder, a user data folder
 * (through XDG_DATA_HOME or APPDATA) and a working directory. The "ROMs" are empty files: only the names count. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app.h"
#include "pc_compat.h"

static int failures;

#define CHECK(cond, ...)                                                                                          \
    do {                                                                                                          \
        if (!(cond)) {                                                                                            \
            failures++;                                                                                           \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
            printf(__VA_ARGS__);                                                                                  \
            printf("\n");                                                                                         \
        }                                                                                                         \
    } while (0)

static char base[PC_APP_PATH], exe[PC_APP_PATH], cwd[PC_APP_PATH], userbase[PC_APP_PATH], user[PC_APP_PATH];

static void make_dir(const char *p)
{
    if (mkdir(p, 0755) != 0 && errno != EEXIST) {
        printf("cannot make %s\n", p);
        exit(1);
    }
}

static void touch(const char *dir, const char *name)
{
    char p[PC_APP_PATH];
    pc_app_join(p, sizeof p, dir, name);
    FILE *f = fopen(p, "wb");
    if (!f) {
        printf("cannot write %s\n", p);
        exit(1);
    }
    fclose(f);
}

static void rm(const char *dir, const char *name)
{
    char p[PC_APP_PATH];
    pc_app_join(p, sizeof p, dir, name);
    remove(p);
}

static void set_user_base(const char *dir)
{
#ifdef _WIN32
    setenv("APPDATA", dir, 1);
#else
    setenv("XDG_DATA_HOME", dir, 1);
#endif
}

static int find(const char *name, int from_command_line, PcAppPlace *places, int *n)
{
    PcAppDirs d;
    pc_app_dirs(&d, exe);
    *n = pc_app_rom_places(&d, name, from_command_line, places);
    return pc_app_rom_find(places, *n);
}

static void expect_path(const PcAppPlace *pl, const char *dir, const char *name)
{
    char want[PC_APP_PATH];
    pc_app_join(want, sizeof want, dir, name);
    CHECK(!strcmp(pl->path, want), "place %s, expected %s", pl->path, want);
}

int main(int argc, char **argv)
{
    PcAppPlace pl[PC_APP_ROM_PLACES];
    PcAppDirs d;
    int n, at;
    const char *rom = "baserom.us.z64";
    if (argc < 2) {
        printf("usage: test_app DIR\n");
        return 2;
    }
    pc_app_join(base, sizeof base, argv[1], "app");
    make_dir(argv[1]);
    make_dir(base);
    pc_app_join(exe, sizeof exe, base, "program");
    pc_app_join(cwd, sizeof cwd, base, "work");
    pc_app_join(userbase, sizeof userbase, base, "data");
    pc_app_join(user, sizeof user, userbase, "rush2049pc");
    make_dir(exe), make_dir(cwd), make_dir(userbase), make_dir(user);
    for (int i = 0; i < 3; i++) {
        const char *dirs[] = {exe, cwd, user};
        rm(dirs[i], rom), rm(dirs[i], "rush2049pc.ini"), rm(dirs[i], "my rom.v64");
    }
    if (chdir(cwd) != 0) {
        printf("cannot enter %s\n", cwd);
        return 1;
    }
    /* the working directory as the program sees it (a symlinked temp folder resolves) */
    pc_app_dirs(&d, exe);
    snprintf(cwd, sizeof cwd, "%s", d.cwd);
    set_user_base(userbase);

    /* The folders: the user data folder; not portable without an ini next to the program */
    pc_app_dirs(&d, exe);
    CHECK(!strcmp(d.user, user), "user folder %s, expected %s", d.user, user);
    CHECK(!d.portable && !strcmp(d.data, user), "installed: data %s, expected %s", d.data, user);
    touch(exe, "rush2049pc.ini");
    pc_app_dirs(&d, exe);
    CHECK(d.portable && !strcmp(d.data, exe), "portable: data %s, expected %s", d.data, exe);
    rm(exe, "rush2049pc.ini");

    /* Nothing anywhere: three places, in order, and none found */
    at = find(rom, 0, pl, &n);
    CHECK(n == 3 && at == -1, "nothing: %d places, found %d", n, at);
    if (n == 3) {
        expect_path(&pl[0], exe, rom), expect_path(&pl[1], user, rom), expect_path(&pl[2], cwd, rom);
        CHECK(!strcmp(pl[0].where, "next to the program") && !strcmp(pl[2].where, "the working directory"),
              "%s, %s", pl[0].where, pl[2].where);
    }
    /* The first place that has it wins */
    touch(cwd, rom);
    CHECK(find(rom, 0, pl, &n) == 2, "only in the working directory");
    touch(user, rom);
    CHECK(find(rom, 0, pl, &n) == 1, "the user data folder before the working directory");
    touch(exe, rom);
    CHECK(find(rom, 0, pl, &n) == 0, "next to the program first");
    rm(exe, rom), rm(user, rom), rm(cwd, rom);

    /* Another name from the settings, with a space; looked up the same way */
    touch(user, "my rom.v64");
    at = find("my rom.v64", 0, pl, &n);
    CHECK(at == 1, "game.rom = my rom.v64 in the user data folder: %d", at);
    rm(user, "my rom.v64");

    /* From the command line (or dropped on the program): as given, one place, no fallback */
    touch(exe, rom);
    at = find("missing.z64", 1, pl, &n);
    CHECK(n == 1 && at == -1 && !strcmp(pl[0].path, "missing.z64") && !strcmp(pl[0].where, "the path given"),
          "--rom missing.z64: %d places, found %d, %s", n, at, pl[0].path);
    touch(cwd, "given.z64");
    CHECK(find("given.z64", 1, pl, &n) == 0, "--rom given.z64, relative to the working directory");
    rm(cwd, "given.z64");
    /* An absolute game.rom: as it is */
    char abs[PC_APP_PATH];
    pc_app_join(abs, sizeof abs, cwd, "elsewhere.z64");
    at = find(abs, 0, pl, &n);
    CHECK(n == 1 && at == -1 && !strcmp(pl[0].where, "the path in the settings (game.rom)"),
          "absolute game.rom: %d places, found %d", n, at);
    rm(exe, rom);

    /* The program run from its own folder: that folder once */
    char saved[PC_APP_PATH];
    snprintf(saved, sizeof saved, "%s", exe);
    snprintf(exe, sizeof exe, "%s", cwd);
    at = find(rom, 0, pl, &n);
    CHECK(n == 2 && at == -1, "program folder = working directory: %d places", n);
    snprintf(exe, sizeof exe, "%s", saved);

#ifndef _WIN32
    /* A relative XDG_DATA_HOME is ignored (~/.local/share); with no HOME either, there is no user folder */
    setenv("XDG_DATA_HOME", "relative/dir", 1);
    setenv("HOME", base, 1);
    pc_app_dirs(&d, exe);
    char want[PC_APP_PATH + 32];
    snprintf(want, sizeof want, "%s/.local/share/rush2049pc", base);
    CHECK(!strcmp(d.user, want), "relative XDG_DATA_HOME: %s, expected %s", d.user, want);
    unsetenv("XDG_DATA_HOME");
    unsetenv("HOME");
    pc_app_dirs(&d, exe);
    CHECK(d.user[0] == 0 && !strcmp(d.data, cwd), "no user folder: data %s", d.data);
    at = find(rom, 0, pl, &n);
    CHECK(n == 2, "no user folder: %d places", n);
#endif

    printf("%s\n", failures ? "app: FAILED" : "app: all passed");
    return failures != 0;
}
