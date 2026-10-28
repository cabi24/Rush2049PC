/* port/config/config.h: rush2049pc.ini, the settings file (docs/port/FRONTEND.md).
 *
 * The settings are resolved in layers, each overriding the one before:
 *   defaults < rush2049pc.ini < environment variables < a press file's `pak` line < command-line options.
 * No SDL here: the binding sections hold names (SDL key and controller names) that port/input resolves. */
#ifndef PC_CONFIG_H
#define PC_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- the INI reader */

/* Lines are `[section]`, `key = value`, or comments starting with ; or #. A ; or # after whitespace starts an
 * inline comment; a value in double quotes keeps everything inside them. Section and key names are given to the
 * handler in lower case, trimmed; the value trimmed. A UTF-8 byte order mark and CRLF line ends are accepted.
 * The handler returns 0 to go on. Returns the number of malformed lines (passed to bad() when it is set). */
typedef int (*PcIniHandler)(void *user, const char *section, const char *key, const char *value, int line);
int pc_ini_parse(const char *text, PcIniHandler handler, void (*bad)(void *user, int line, const char *why),
                 void *user);

/* ---------------------------------------------------------------- N64 controls and hotkeys */

/* The controls a binding can name, in [keyboard] and [controller]. */
enum {
    PC_CTL_A, PC_CTL_B, PC_CTL_Z, PC_CTL_START,
    PC_CTL_DUP, PC_CTL_DDOWN, PC_CTL_DLEFT, PC_CTL_DRIGHT,
    PC_CTL_L, PC_CTL_R,
    PC_CTL_CUP, PC_CTL_CDOWN, PC_CTL_CLEFT, PC_CTL_CRIGHT,
    PC_CTL_STICK_UP, PC_CTL_STICK_DOWN, PC_CTL_STICK_LEFT, PC_CTL_STICK_RIGHT,
    PC_NUM_CONTROLS
};
extern const char *const pc_control_names[PC_NUM_CONTROLS];   /* "A", "CUP", "STICK_LEFT", ... */
extern const uint16_t pc_control_masks[PC_NUM_CONTROLS];      /* N64 button bits; 0 for the stick */
int pc_control_find(const char *name);                        /* index, case-insensitive; -1 if none */

enum { PC_HOTKEY_FULLSCREEN, PC_HOTKEY_SCREENSHOT, PC_HOTKEY_PAUSE, PC_HOTKEY_QUIT, PC_HOTKEY_MENU, PC_NUM_HOTKEYS };
extern const char *const pc_hotkey_names[PC_NUM_HOTKEYS];     /* "fullscreen", ..., "menu" (the settings overlay) */

/* ---------------------------------------------------------------- settings */

enum { PC_CFG_PAK_NONE = 0, PC_CFG_PAK_MEMORY = 1, PC_CFG_PAK_RUMBLE = 2 };   /* = port/ultra's PC_PAK_* */
enum { PC_CFG_API_AUTO = 0, PC_CFG_API_GL = 1, PC_CFG_API_GLES = 2 };   /* the order of the file's choices */
enum { PC_CFG_STICK_LEFT = 0, PC_CFG_STICK_RIGHT = 1, PC_CFG_STICK_NONE = 2 };

#define PC_CFG_PATH 1024
#define PC_CFG_BIND 128

typedef struct PcSettings {
    /* [game] */
    char rom[PC_CFG_PATH];          /* the ROM (.z64/.v64/.n64) */
    int rom_check;                  /* refuse a ROM whose SHA-1 differs */
    int expansion_pak;              /* 8 MiB of RDRAM */
    int pak;                        /* PC_CFG_PAK_*: controller 1's accessory */
    char pak_file[PC_CFG_PATH];     /* the controller pak image */
    /* [video] */
    int width, height;              /* window size */
    int fullscreen;                 /* desktop fullscreen */
    int vsync;
    int frame_rate;                 /* frames shown a second: 0 the game's own, -1 the display's (RUSH_GFX_FRAME_RATE) */
    int scale;                      /* internal resolution multiple (RUSH_GFX_SCALE); 0: auto, the window's */
    int api;                        /* PC_CFG_API_* (RUSH_GFX_API) */
    double widescreen;              /* display aspect the 4:3 frame widens to: 0 off, -1 the window's (auto) */
    int pause_on_focus_loss;
    char screenshot_dir[PC_CFG_PATH];   /* empty: the working directory */
    /* [audio] */
    char audio[16];                 /* auto, sdl, alsa, null, wav (RUSH2049_AUDIO) */
    int volume;                     /* master volume, 0-100 */
    int audio_latency;              /* ms of sound the output keeps queued at least; 0: from the device */
    /* [input] */
    int keyboard_player;            /* 1-4, 0: no keyboard */
    int first_controller_player;    /* controllers fill players from here, in the order they are plugged in */
    double deadzone;                /* radial, for sticks; also triggers */
    double sensitivity;             /* the stick's travel past the dead zone is multiplied by this (0.5-2) */
    double axis_threshold;          /* a trigger or a stick axis bound to a button presses it past this */
    int stick_range;                /* N64 stick value at full tilt, left/right/up/down */
    int stick_diagonal;             /* each axis at full diagonal tilt (the octagonal gate); 0: a round gate */
    double rumble;                  /* rumble strength 0-1 */
    /* [keyboard], [controller], [hotkeys]: comma-separated names, empty for none */
    char key[PC_NUM_CONTROLS][PC_CFG_BIND];
    int analog;                     /* PC_CFG_STICK_*: which controller stick is the N64 stick */
    char pad[PC_NUM_CONTROLS][PC_CFG_BIND];
    char pad_menu[PC_CFG_BIND];     /* [controller] menu: the buttons that open the settings overlay */
    char hotkey[PC_NUM_HOTKEYS][PC_CFG_BIND];

    /* Not in the file: where it was read from ("" for none) and its directory; where the settings overlay saves
     * (pc_settings_resolve: the file read, the --config path, or the located one; "" for nowhere). */
    char source[PC_CFG_PATH];
    char source_dir[PC_CFG_PATH];
    char save_path[PC_CFG_PATH];
} PcSettings;

/* One setting by name, as the file has it: ("video", "scale", "2"). */
typedef struct {
    const char *section, *key, *value;
} PcSettingValue;

void pc_settings_defaults(PcSettings *s);
/* Set one setting from its text. 0 on success; -1 for an unknown section or key, -2 for a bad value (the
 * setting is unchanged). why gets a message when it is set. */
int pc_settings_set(PcSettings *s, const char *section, const char *key, const char *value, char *why,
                    size_t why_size);
/* "section.key=value" (--set). Same results; -3 when it isn't of that form. */
int pc_settings_assign(PcSettings *s, const char *assignment, char *why, size_t why_size);
/* An INI text over the settings. Problems are reported to stderr with name (a path) and line, and that line is
 * skipped. Returns how many. */
int pc_settings_load_text(PcSettings *s, const char *text, const char *name);
/* The same for a file: -1 when it can't be read. Records source and source_dir. */
int pc_settings_load_file(PcSettings *s, const char *path);
/* The environment variables that other parts already read: RUSH2049_AUDIO, RUSH_GFX_SCALE, RUSH_GFX_API,
 * RUSH_GFX_WIDESCREEN. Bad values are reported and ignored. Returns how many were bad. */
int pc_settings_apply_env(PcSettings *s);
/* The whole file, with comments, as malloc'd text. */
char *pc_settings_format(const PcSettings *s);
/* One setting's value as the file writes it ("16:9", "on", "X, Left Shift", ...): the bindings and hotkeys too.
 * -1 for an unknown setting. */
int pc_settings_get(const PcSettings *s, const char *section, const char *key, char *out, size_t out_size);
int pc_settings_write(const PcSettings *s, const char *path);   /* through a temp file and rename; 0 on success */
/* The file over s without reporting its problems (bad lines are skipped). -1 when it can't be read. */
int pc_settings_read_quiet(PcSettings *s, const char *path);

/* An INI text with settings changed in place (the settings overlay's Save). Each change replaces the value of the
 * last line that sets it, keeping that line's key, spacing and inline comment; a setting the text lacks goes after
 * the last setting of its section, and a section it lacks goes at the end. Everything else (comments, blank lines,
 * order, unknown keys, bad lines, a BOM, CRLF line ends) is kept byte for byte. Values are quoted when they need it
 * to read back the same. malloc'd. */
char *pc_ini_update(const char *text, const PcSettingValue *changes, int n);
/* The changes saved to the settings file at path: pc_ini_update over it when it exists, otherwise the whole file
 * (pc_settings_format) of the defaults with the changes, its directory made. Through a temp file and rename.
 * 0 on success; why gets a message otherwise. */
int pc_settings_save(const char *path, const PcSettingValue *changes, int n, char *why, size_t why_size);

/* Where the file is: rush2049pc.ini next to the executable when there is one (a portable install), else
 * $XDG_CONFIG_HOME/rush2049pc/rush2049pc.ini (~/.config/... without XDG_CONFIG_HOME). *exists says whether that
 * file exists. -1 when there is no place at all (no exe_dir file, no XDG_CONFIG_HOME and no HOME). */
int pc_config_locate(char *out, size_t out_size, const char *exe_dir, int *exists);
/* The directory of the running executable ("" if unknown). */
void pc_config_exe_dir(char *out, size_t out_size, const char *argv0);

/* All layers: defaults; then the file; then the environment; then the overrides in order. The file is path when
 * it is set, else the located one. use_file: PC_CFG_FILE_NONE skips it, PC_CFG_FILE_READ reads it if it exists,
 * PC_CFG_FILE_CREATE also writes a missing located file with the defaults first (the first run). Returns 0, or
 * -1 when an override is bad (reported to stderr; the others still apply). */
enum { PC_CFG_FILE_NONE = 0, PC_CFG_FILE_READ = 1, PC_CFG_FILE_CREATE = 2 };
int pc_settings_resolve(PcSettings *s, int use_file, const char *path, const char *exe_dir,
                        const PcSettingValue *overrides, int n_overrides);

#ifdef __cplusplus
}
#endif

#endif
