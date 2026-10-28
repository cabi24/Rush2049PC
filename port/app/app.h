/* port/app/app.h: the program around the game (docs/port/RELEASE.md): where its files are, finding the ROM and
 * saying what is wrong with it, message boxes. paths.c has no SDL and no port/ultra, so the tests link it alone. */
#ifndef PC_APP_H
#define PC_APP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_APP_PATH 1024
#ifdef _WIN32
#define PC_APP_SEP '\\'
#else
#define PC_APP_SEP '/'
#endif

/* ---------------------------------------------------------------- paths.c: folders */

typedef struct PcAppDirs {
    char exe[PC_APP_PATH];    /* the program's folder; "" if unknown */
    char user[PC_APP_PATH];   /* the user data folder: $XDG_DATA_HOME/rush2049pc (~/.local/share/rush2049pc), or
                               * %APPDATA%\rush2049pc on Windows; "" if there is none */
    char cwd[PC_APP_PATH];    /* the working directory; "" if unknown */
    int portable;             /* rush2049pc.ini is next to the program: everything stays next to it */
    char data[PC_APP_PATH];   /* where the program keeps what it writes: exe when portable, else user (else cwd) */
} PcAppDirs;

/* The folders, with exe_dir the program's (pc_config_exe_dir). */
void pc_app_dirs(PcAppDirs *d, const char *exe_dir);
/* dir + separator + name into out (name alone when dir is empty). */
void pc_app_join(char *out, size_t size, const char *dir, const char *name);
int pc_app_is_absolute(const char *path);
int pc_app_exists(const char *path);   /* anything there (a file, a folder) */
void pc_app_dirname(const char *path, char *out, size_t size);   /* "." for a bare name */
int pc_app_mkdirs(const char *dir);    /* the folder and its parents; 0 when it is there */

/* ---------------------------------------------------------------- paths.c: what the program writes */

/* A file the program writes, named by a setting (game.pak_file, video.screenshot_dir): value as it is when it came
 * from the command line (from the working directory) or is absolute, otherwise in the data folder. */
void pc_app_data_file(const PcAppDirs *d, const char *value, int from_command_line, char *out, size_t size);
/* The controller pak's file, the same way; except that when it isn't there and the working directory has one by
 * that name, where earlier builds kept it, that one is used. Returns 1 then. */
int pc_app_pak_file(const PcAppDirs *d, const char *value, int from_command_line, char *out, size_t size);

/* ---------------------------------------------------------------- paths.c: where the ROM is looked for */

#define PC_APP_ROM_PLACES 4
typedef struct {
    char path[PC_APP_PATH];
    const char *where;   /* "next to the program", ... */
} PcAppPlace;

/* The places the ROM is looked for, in order, each once: name (game.rom) as it is when it came from the command
 * line or is absolute; otherwise name in the program's folder, the user data folder and the working directory.
 * Returns how many (0 only for an empty name). */
int pc_app_rom_places(const PcAppDirs *d, const char *name, int from_command_line, PcAppPlace out[PC_APP_ROM_PLACES]);
/* The first of them where something exists, or -1. */
int pc_app_rom_find(const PcAppPlace *places, int n);

/* ---------------------------------------------------------------- alert.c: message boxes */

/* A message box, when show is set: SDL2's, or Windows' own in a build without SDL2. RUSH2049_MESSAGE_BOX=print
 * prints it to stderr instead ("rush2049pc: message box: TITLE" and the text), =off shows none. 0 when it was
 * shown or printed. The text is also for the console, so the caller prints its own short message too. */
int pc_app_alert(int show, const char *title, const char *text);
/* The same with Windows' own MessageBoxA there (no SDL: for a fault, inside the exception filter). */
int pc_app_alert_native(int show, const char *title, const char *text);
enum { PC_APP_BOX_ERROR, PC_APP_BOX_WARNING, PC_APP_BOX_INFO };
int pc_app_alert_kind(int show, int kind, const char *title, const char *text);   /* with that icon */
/* A box with up to 4 buttons (the first is Enter's, the last Escape's and the close button's): the index of the one
 * pressed, or -1 when no box could ask (not show, RUSH2049_MESSAGE_BOX=print or off, no display). With
 * RUSH2049_TEST_ROM_PICK the box is printed with its buttons, and the answer is the first button while the hook has
 * answers left for the file dialog, else the last. */
int pc_app_ask(int show, int kind, const char *title, const char *text, const char *const *buttons, int n);
int pc_app_box_interactive(int show);   /* pc_app_ask would get an answer: a real box, or the test hook */

/* ---------------------------------------------------------------- pick.c: the "Open ROM" file dialog */

/* A file dialog can be shown: Windows' (IFileOpenDialog), zenity or kdialog on Linux, or the test hook. */
int pc_app_pick_available(void);
/* The dialog: 0 with the file's path in out, 1 when the player cancelled, -1 when no dialog could be shown, 2 when
 * the file chosen can't be named in the program's paths (Windows: letters outside the code page), out saying so. The
 * test hook, RUSH2049_TEST_ROM_PICK="ANSWER|ANSWER|...", answers from its list instead: a path, or :cancel, or
 * :fail (no dialog), or :dialog (the real one, for a stand-in zenity on PATH). */
int pc_app_pick_rom(char *out, size_t size);
int pc_app_pick_test(void);      /* RUSH2049_TEST_ROM_PICK is set */
int pc_app_pick_pending(void);   /* ... and has answers left */

/* ---------------------------------------------------------------- startup.c: the console, the process */

/* First thing in main. Windows: a program started without standard handles (rush2049pc.exe is a GUI program)
 * attaches to its parent's console and prints there, or, with none to attach to, keeps what it prints in memory
 * (quiet); then the timer period and power throttling for the run. RUSH2049_CONSOLE=none: quiet on any system. */
void pc_app_startup(void);
int pc_app_console_quiet(void);   /* nobody sees what the program prints: errors need a message box */
/* The last lines printed (quiet only; "" otherwise): up to 12. */
void pc_app_console_tail(char *out, size_t size);
int pc_app_console_print_fd(void);   /* where RUSH2049_MESSAGE_BOX=print writes: the real stderr */
void pc_app_box_shown(void);         /* alert.c: a box was shown (or printed) */
int pc_app_box_was_shown(void);

/* ---------------------------------------------------------------- icon.c: the window's icon */

/* The program's icon on the SDL window (Linux; on Windows the window takes the executable's icon resource). */
void pc_app_window_icon(void);

/* ---------------------------------------------------------------- log.c: the version, the log, the end */

const char *pc_app_version(void);                   /* git describe at build time (cmake/version.cmake) */
const char *pc_app_version_commit(void);            /* the full commit hash, or "unknown" */
const char *pc_app_version_build(void);             /* "local build", or "GitHub Actions run N (owner/repo)" */
const char *pc_app_build_type(void);                /* CMake's build type: Debug, RelWithDebInfo, ...; "unknown" */
void pc_app_version_text(char *out, size_t size);   /* --version: the version, commit, build, platform */
/* The run's output into path as well as the console, from now on; the run before is kept as *.old.log. The log
 * starts with the version, the time, the folders and the command line. 0 when it is open; otherwise a warning is
 * printed and the program runs without it. */
int pc_app_log_open(const char *path, int argc, char **argv, const PcAppDirs *d);
void pc_app_log_close(void);         /* everything printed is in the file; stderr is the console again */
const char *pc_app_log_path(void);   /* NULL without a log */
void pc_app_log_window(int window);  /* the program has a window: a fault shows a message box (Windows) */
/* port/ultra's pc_on_fault: the log closed; on Windows a message box saying where it is (with a window), and a
 * wait for a key when the program owns its console. */
void pc_app_on_fault(const char *what);
/* The end of a run with this exit code: the log closed; on Windows, after an error, the wait for a key. */
void pc_app_finish(int code);

/* ---------------------------------------------------------------- rom.c: the ROM at startup */

/* Finds the ROM by the lookup order and loads it (port/ultra's pc_rom_open). On a problem, prints a short reason
 * and what to do, and shows the same in a message box when window is set; when a file dialog can be shown, the box
 * offers one ("Open ROM..."), and the file chosen is checked the same way, and offered again until it is usable or
 * the player quits. config is the settings file's path, for the message; a file chosen is kept as game.rom in
 * remember (the settings file read; NULL or "" for none). 0 when the cartridge is loaded. */
int pc_app_load_rom(const PcAppDirs *d, const char *name, int from_command_line, int check_sha1, const char *config,
                    const char *remember, int window);
/* The ROM pc_app_load_rom loaded ("" before, or without one), and *where the place it was found ("next to the
 * program", ..., "chosen in the file dialog") when where is set. */
const char *pc_app_rom_path(const char **where);

/* ---------------------------------------------------------------- saves.c: the controller pak's file at start */

/* Before the game reads a pak file (pc_si_init), for each pak file there is (any player's): one the game would write
 * over (unreadable, the wrong size, a damaged ID block or file table) is renamed PATH.corrupt-DATE, and the newest
 * good backup, if any, put in its place; a good one is copied to PATH.bak when it differs from it, the older
 * backups moved to PATH.bak2 and PATH.bak3. Says what it did on the console, and in a message box (when window) for
 * a pak set aside. Only the file at path and its own backups are touched. Returns PC_APP_SAVES_*. */
enum { PC_APP_SAVES_OK, PC_APP_SAVES_NEW, PC_APP_SAVES_BACKED_UP, PC_APP_SAVES_RESTORED, PC_APP_SAVES_SET_ASIDE,
       PC_APP_SAVES_LEFT, PC_APP_SAVES_N64 };
int pc_app_saves_prepare(const char *path, int window);
#define PC_APP_BACKUPS 3

#ifdef __cplusplus
}
#endif

#endif
