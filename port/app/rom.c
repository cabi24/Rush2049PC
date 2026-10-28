/* port/app/rom.c: the ROM at startup (docs/port/RELEASE.md, "The ROM"). The lookup order is paths.c's; what a file
 * is, romcheck.c's. When there is no usable ROM, the console gets a short reason and what to do, and a message box
 * the same at more length when the program would open a window. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "ultra.h"

#ifdef _WIN32
#define EXE "rush2049pc.exe"
#define HOW_TO_NAME "or drag the ROM file onto " EXE
#else
#define EXE "rush2049pc"
#define HOW_TO_NAME "or start the program with: " EXE " --rom PATH"
#endif
#define NEEDED "San Francisco Rush 2049 (USA)"

typedef struct {
    char *p;
    size_t n, size;
} Text;

static void add(Text *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void add(Text *t, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(t->p + t->n, t->size - t->n, fmt, ap);
    va_end(ap);
    if (k > 0)
        t->n = t->n + (size_t)k < t->size ? t->n + (size_t)k : t->size - 1;
}

static void not_found(const PcAppDirs *d, const PcAppPlace *places, int n, const char *name, int from_command_line,
                      const char *config, int window)
{
    char box[4096];
    Text t = {box, 0, sizeof box};
    box[0] = 0;
    const char *here = d->exe[0] ? "next to the program" : "into the working directory";
    if (from_command_line || n == 1) {
        /* a path given: it is final, so the advice is to check it */
        int relative = !pc_app_is_absolute(places[0].path);
        fprintf(stderr, "rush2049pc: no ROM at %s (%s%s%s): there is no such file\n", places[0].path, places[0].where,
                relative ? "; the working directory is " : "", relative ? d->cwd : "");
        add(&t, "There is no ROM at\n    %s\n(%s%s%s).\n\nCheck the path and the file's name. Without a path, the "
                "program looks for baserom.us.z64 %s.", places[0].path, places[0].where,
            relative ? ", from the working directory " : "", relative ? d->cwd : "", here);
    } else {
        fprintf(stderr, "rush2049pc: no ROM: %s is in none of these places:\n", name);
        add(&t, "This program plays your own copy of the " NEEDED " cartridge, from a ROM file, and none was "
                "found.\n\nIt looked for %s in:\n", name);
        for (int i = 0; i < n; i++) {
            fprintf(stderr, "rush2049pc:   %s (%s)\n", places[i].path, places[i].where);
            add(&t, "    %s (%s)\n", places[i].path, places[i].where);
        }
        fprintf(stderr, "rush2049pc: copy your " NEEDED " ROM %s as %s, or give its path: " EXE " --rom PATH\n",
                here, name);
        add(&t, "\nCopy your ROM %s and name it %s, " HOW_TO_NAME ". .z64, .v64 and .n64 files all work.", here,
            name);
        if (config && *config)
            add(&t, "\n\nOr set game.rom to its path in\n    %s", config);
    }
    pc_app_alert(window, "Rush 2049 PC: no ROM", box);
}

static void unusable(const char *path, const char *where, const PcRomInfo *info, int window)
{
    char box[4096];
    Text t = {box, 0, sizeof box};
    int other = info->status == PC_ROM_OTHER_VERSION || info->status == PC_ROM_BAD_DUMP;   /* Rush 2049 */
    box[0] = 0;
    fprintf(stderr, "rush2049pc: the ROM %s (%s) can't be used: %s\n", path, where, info->what);
    add(&t, "The ROM\n    %s\n(%s) can't be used: %s.\n\n", path, where, info->what);
    if (info->sha1[0]) {
        fprintf(stderr, "rush2049pc: its SHA-1 is %s; " NEEDED " is %s\n", info->sha1, PC_ROM_SHA1);
        add(&t, "Its SHA-1 is %s.\n", info->sha1);
    }
    add(&t, "This program needs " NEEDED ", SHA-1 %s.", PC_ROM_SHA1);
    if (other) {
        fprintf(stderr, "rush2049pc: --no-rom-check runs it anyway (it will probably not work)\n");
        add(&t, "\n\nTo try it anyway, start the program with --no-rom-check. It will probably not work.");
    }
    pc_app_alert(window, "Rush 2049 PC: wrong ROM", box);
}

/* The ROM loaded, for the settings overlay's About page. */
static char s_rom[PC_APP_PATH], s_rom_where[64];

const char *pc_app_rom_path(const char **where)
{
    if (where)
        *where = s_rom_where;
    return s_rom;
}

static void loaded(const PcAppPlace *place)
{
    snprintf(s_rom, sizeof s_rom, "%s", place->path);
    snprintf(s_rom_where, sizeof s_rom_where, "%s", place->where);
}

int pc_app_load_rom(const PcAppDirs *d, const char *name, int from_command_line, int check_sha1, const char *config,
                    int window)
{
    PcAppPlace places[PC_APP_ROM_PLACES];
    PcRomInfo info;
    int n = pc_app_rom_places(d, name, from_command_line, places);
    if (n == 0) {
        fprintf(stderr, "rush2049pc: no ROM: game.rom is empty\n");
        pc_app_alert(window, "Rush 2049 PC: no ROM", "The setting game.rom is empty: it names the ROM file.");
        return -1;
    }
    int at = pc_app_rom_find(places, n);
    if (at < 0) {
        not_found(d, places, n, name, from_command_line, config, window);
        return -1;
    }
    int status = pc_rom_open(places[at].path, check_sha1, &info);
    if (status == PC_ROM_OK) {
        fprintf(stderr, "rush2049pc: ROM: %s (%s; .%s)\n", places[at].path, places[at].where, info.order);
        loaded(&places[at]);
        return 0;
    }
    if (status == PC_ROM_MISSING) {   /* gone between the two looks */
        not_found(d, places + at, 1, name, 1, config, window);
        return -1;
    }
    if (pc_rom_bytes()) {   /* --no-rom-check */
        fprintf(stderr, "rush2049pc: warning: the ROM %s (%s): %s; running it anyway (--no-rom-check)\n",
                places[at].path, places[at].where, info.what);
        loaded(&places[at]);
        return 0;
    }
    unusable(places[at].path, places[at].where, &info, window);
    return -1;
}
