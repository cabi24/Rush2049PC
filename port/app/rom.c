/* port/app/rom.c: the ROM at startup (docs/port/RELEASE.md, "The ROM"). The lookup order is paths.c's; what a file
 * is, romcheck.c's. When there is no usable ROM, the console gets a short reason and what to do, and a message box
 * the same at more length when the program would open a window. When a file dialog can be shown (pick.c), the box
 * offers it: the file chosen is checked as any other, offered again while it can't be used, and kept as game.rom in
 * the settings file when it can, so the next start finds it. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "config.h"
#include "ultra.h"

#ifdef _WIN32
#define EXE "rush2049pc.exe"
#define HOW_TO_NAME "or drag the ROM file onto " EXE
#else
#define EXE "rush2049pc"
#define HOW_TO_NAME "or start the program with: " EXE " --rom PATH"
#endif
#define NEEDED "San Francisco Rush 2049 (USA)"
#define OPEN "Open ROM..."
#define AGAIN "Choose another..."
#define QUIT "Quit"

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

/* What the box says about the file dialog, when it offers one. */
static void add_offer(Text *t, const char *before, const char *remember)
{
    add(t, "%s" OPEN " lets you choose the ROM file now%s.", before, remember && *remember ?
        "; the program checks it and keeps where it is in its settings (game.rom), so it finds it from then on" :
        "");
}

/* No file: the console's lines (when print is set) and the box's text into box. */
static void not_found(const PcAppDirs *d, const PcAppPlace *places, int n, const char *name, int from_command_line,
                      const char *config, int print, int offer, const char *remember, char *box, size_t size)
{
    Text t = {box, 0, size};
    box[0] = 0;
    const char *here = d->exe[0] ? "next to the program" : "into the working directory";
    if (from_command_line || n == 1) {
        /* a path given: it is final, so the advice is to check it */
        int relative = !pc_app_is_absolute(places[0].path);
        if (print)
            fprintf(stderr, "rush2049pc: no ROM at %s (%s%s%s): there is no such file\n", places[0].path,
                    places[0].where, relative ? "; the working directory is " : "", relative ? d->cwd : "");
        add(&t, "There is no ROM at\n    %s\n(%s%s%s).\n\nCheck the path and the file's name. Without a path, the "
                "program looks for baserom.us.z64 %s.", places[0].path, places[0].where,
            relative ? ", from the working directory " : "", relative ? d->cwd : "", here);
        if (offer)
            add_offer(&t, "\n\n", remember);
        return;
    }
    if (print) {
        fprintf(stderr, "rush2049pc: no ROM: %s is in none of these places:\n", name);
        for (int i = 0; i < n; i++)
            fprintf(stderr, "rush2049pc:   %s (%s)\n", places[i].path, places[i].where);
        fprintf(stderr, "rush2049pc: copy your " NEEDED " ROM %s as %s, or give its path: " EXE " --rom PATH\n",
                here, name);
    }
    add(&t, "This program plays your own copy of the " NEEDED " cartridge, from a ROM file, and none was "
            "found.\n\nIt looked for %s in:\n", name);
    for (int i = 0; i < n; i++)
        add(&t, "    %s (%s)\n", places[i].path, places[i].where);
    if (offer) {
        add_offer(&t, "\n", remember);
        add(&t, " .z64, .v64 and .n64 files all work.\n\nOr copy your ROM %s and name it %s, " HOW_TO_NAME ".", here,
            name);
        return;
    }
    add(&t, "\nCopy your ROM %s and name it %s, " HOW_TO_NAME ". .z64, .v64 and .n64 files all work.", here, name);
    if (config && *config)
        add(&t, "\n\nOr set game.rom to its path in\n    %s", config);
}

/* A file that can't be used, the same way. */
static void unusable(const char *path, const char *where, const PcRomInfo *info, int print, int offer, char *box,
                     size_t size)
{
    Text t = {box, 0, size};
    int other = info->status == PC_ROM_OTHER_VERSION || info->status == PC_ROM_BAD_DUMP;   /* Rush 2049 */
    box[0] = 0;
    if (print)
        fprintf(stderr, "rush2049pc: the ROM %s (%s) can't be used: %s\n", path, where, info->what);
    add(&t, "The ROM\n    %s\n(%s) can't be used: %s.\n\n", path, where, info->what);
    if (info->sha1[0]) {
        if (print)
            fprintf(stderr, "rush2049pc: its SHA-1 is %s; " NEEDED " is %s\n", info->sha1, PC_ROM_SHA1);
        add(&t, "Its SHA-1 is %s.\n", info->sha1);
    }
    add(&t, "This program needs " NEEDED ", SHA-1 %s.", PC_ROM_SHA1);
    if (other) {
        if (print)
            fprintf(stderr, "rush2049pc: --no-rom-check runs it anyway (it will probably not work)\n");
        add(&t, "\n\nTo try it anyway, start the program with --no-rom-check. It will probably not work.");
    }
    if (offer)
        add(&t, "\n\nYou can choose another file, or quit.");
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

static void remember_rom(const char *remember, const char *path)
{
    char why[PC_APP_PATH + 64];
    PcSettingValue change = {"game", "rom", path};
    if (!remember || !*remember) {
        fprintf(stderr, "rush2049pc: the ROM chosen is not kept: no settings file (--no-config)\n");
        return;
    }
    if (strchr(path, '\n') || path[0] == '"') {
        fprintf(stderr, "rush2049pc: the ROM chosen is not kept: its path can't be written in the settings\n");
        return;
    }
    if (pc_settings_save(remember, &change, 1, why, sizeof why) == 0)
        fprintf(stderr, "rush2049pc: game.rom = %s kept in %s, for the next start\n", path, remember);
    else
        fprintf(stderr, "rush2049pc: the ROM chosen is not kept: %s\n", why);
}

/* The box with its question, the file dialog, and the file chosen checked, until one can be used (0) or the player
 * quits (-1). box (the problem so far, with the offer) and fallback (the same without it, for when no dialog
 * can be shown) are the first box's. */
static int choose(const PcAppDirs *d, int check_sha1, const char *remember, const char *title, char *box,
                  const char *fallback)
{
    const char *buttons[2] = {OPEN, QUIT};
    char box_title[64];
    snprintf(box_title, sizeof box_title, "%s", title);
    for (;;) {
        int answer = pc_app_ask(1, PC_APP_BOX_ERROR, box_title, box, buttons, 2);
        if (answer < 0) {
            /* the box couldn't ask (SDL failed): the problem's lines are on the console already */
            return -1;
        }
        if (answer != 0) {
            fprintf(stderr, "rush2049pc: no ROM chosen (%s)\n", QUIT);
            return -1;
        }
        char picked[PC_APP_PATH], path[PC_APP_PATH];
        int rc = pc_app_pick_rom(picked, sizeof picked);
        if (rc == 1) {
            fprintf(stderr, "rush2049pc: no ROM chosen (the file dialog was cancelled)\n");
            return -1;
        }
        if (rc < 0) {
            fprintf(stderr, "rush2049pc: no file dialog could be shown\n");
            char text[4096];
            snprintf(text, sizeof text, "No file dialog could be shown.\n\n%s", fallback);
            pc_app_alert(1, box_title, text);
            return -1;
        }
        buttons[0] = AGAIN;
        snprintf(box_title, sizeof box_title, "Rush 2049 PC: wrong ROM");
        if (rc == 2) {
            snprintf(box, 4096, "The file chosen can't be opened: %s.\n\nYou can choose another file, or quit.",
                     picked);
            continue;
        }
        if (pc_app_is_absolute(picked) || !d->cwd[0])
            snprintf(path, sizeof path, "%s", picked);
        else
            pc_app_join(path, sizeof path, d->cwd, picked);
        PcRomInfo info;
        int status = pc_rom_open(path, check_sha1, &info);
        if (status == PC_ROM_OK || pc_rom_bytes()) {
            if (status == PC_ROM_OK)
                fprintf(stderr, "rush2049pc: ROM: %s (chosen; .%s)\n", path, info.order);
            else
                fprintf(stderr, "rush2049pc: warning: the ROM %s (chosen; .%s): %s; running it anyway "
                        "(--no-rom-check)\n", path, info.order, info.what);
            PcAppPlace place = {"", "chosen in the file dialog"};
            snprintf(place.path, sizeof place.path, "%s", path);
            loaded(&place);
            remember_rom(remember, path);
            return 0;
        }
        unusable(path, "the file chosen", &info, 1, 1, box, 4096);
    }
}

int pc_app_load_rom(const PcAppDirs *d, const char *name, int from_command_line, int check_sha1, const char *config,
                    const char *remember, int window)
{
    PcAppPlace places[PC_APP_ROM_PLACES];
    PcRomInfo info;
    static char box[4096], fallback[4096];
    const char *title;
    int offer = pc_app_box_interactive(window) && pc_app_pick_available();
    int n = pc_app_rom_places(d, name, from_command_line, places);
    if (n == 0) {
        fprintf(stderr, "rush2049pc: no ROM: game.rom is empty\n");
        pc_app_alert(window, "Rush 2049 PC: no ROM", "The setting game.rom is empty: it names the ROM file.");
        return -1;
    }
    int at = pc_app_rom_find(places, n);
    int status = at < 0 ? PC_ROM_MISSING : pc_rom_open(places[at].path, check_sha1, &info);
    if (status == PC_ROM_OK) {
        fprintf(stderr, "rush2049pc: ROM: %s (%s; .%s)\n", places[at].path, places[at].where, info.order);
        loaded(&places[at]);
        return 0;
    }
    if (status == PC_ROM_MISSING) {   /* none there, or gone between the two looks */
        const PcAppPlace *p = at < 0 ? places : places + at;
        int k = at < 0 ? n : 1, given = at < 0 ? from_command_line : 1;
        title = "Rush 2049 PC: no ROM";
        not_found(d, p, k, name, given, config, 1, offer, remember, box, sizeof box);
        if (offer)
            not_found(d, p, k, name, given, config, 0, 0, remember, fallback, sizeof fallback);
    } else if (pc_rom_bytes()) {   /* --no-rom-check */
        fprintf(stderr, "rush2049pc: warning: the ROM %s (%s): %s; running it anyway (--no-rom-check)\n",
                places[at].path, places[at].where, info.what);
        loaded(&places[at]);
        return 0;
    } else {
        title = "Rush 2049 PC: wrong ROM";
        unusable(places[at].path, places[at].where, &info, 1, offer, box, sizeof box);
        if (offer)
            unusable(places[at].path, places[at].where, &info, 0, 0, fallback, sizeof fallback);
    }
    if (!offer) {
        pc_app_alert(window, title, box);
        return -1;
    }
    return choose(d, check_sha1, remember, title, box, fallback);
}
