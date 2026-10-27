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

/* ---------------------------------------------------------------- rom.c: the ROM at startup */

/* Finds the ROM by the lookup order and loads it (port/ultra's pc_rom_open). On a problem, prints a short reason
 * and what to do, and shows the same in a message box when window is set. config is the settings file's path,
 * for the message. 0 when the cartridge is loaded. */
int pc_app_load_rom(const PcAppDirs *d, const char *name, int from_command_line, int check_sha1, const char *config,
                    int window);

#ifdef __cplusplus
}
#endif

#endif
