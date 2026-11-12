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

/* = port/ultra's PC_PAK_*; auto: a rumble pak while the player's controller can rumble, else none */
enum { PC_CFG_PAK_NONE = 0, PC_CFG_PAK_MEMORY = 1, PC_CFG_PAK_RUMBLE = 2, PC_CFG_PAK_AUTO = 3 };
enum { PC_CFG_API_AUTO = 0, PC_CFG_API_GL = 1, PC_CFG_API_GLES = 2 };   /* the order of the file's choices */
enum { PC_CFG_SAVES_PAK = 0, PC_CFG_SAVES_FILES = 1 };   /* game.saves (docs/port/SAVES.md) */
enum { PC_CFG_STICK_LEFT = 0, PC_CFG_STICK_RIGHT = 1, PC_CFG_STICK_NONE = 2 };
enum { PC_STEER_INSTANT = 0, PC_STEER_SMOOTH = 1 };   /* input.keyboard_steering */

#define PC_CFG_PATH 1024
#define PC_CFG_BIND 128
#define PC_CFG_MODS 64                  /* [mods] lines kept (docs/port/MODS.md) */

typedef struct PcSettings {
    /* [game] */
    char rom[PC_CFG_PATH];          /* the ROM (.z64/.v64/.n64) */
    int rom_check;                  /* refuse a ROM whose SHA-1 differs */
    int expansion_pak;              /* 8 MiB of RDRAM */
    int cars;                       /* the most cars a race holds: 0 original (the N64's 6), else 6-12 */
    int memory;                     /* game.memory: 0 original (the N64's: 8 MiB, 4 with expansion_pak off), else
                                     * osMemSize in MiB, PC_MEMORY_MIN_MIB to PC_MEMORY_MAX_MIB (docs/port/MEMORY.md) */
    int game_fps;                   /* game.frame_rate: 0 original (a frame every two retraces at most), 60 */
    int pc_setup;                   /* game.pc_setup: PC SETUP in the main menu opens the overlay (start_menu.c) */
    int ghost_prompt;               /* game.ghost_prompt: the ghost save prompt for every profile (ghost_prompt.c) */
    int cheats_menu;                /* game.cheats_menu: the main menu's CHEATS option without its code (cheats.c) */
    int cheats_unlock;              /* game.cheats_unlock: every cheat on the cheat screen available (cheats.c) */
    /* game.mods, mods_dir, mods_order and [mods] (docs/port/MODS.md) */
    int mods;                       /* use the mods folder's mods */
    char mods_dir[PC_CFG_PATH];     /* the mods folder; a name alone is in the data folder */
    char mods_order[PC_CFG_PATH];   /* folder names, comma-separated, first first; the rest by mod.ini's priority */
    int nmod_flags;                 /* [mods] folder = on|off: a mod not listed is on */
    struct {
        char name[64];
        int on;
    } mod_flag[PC_CFG_MODS];
    int pak[4];                     /* PC_CFG_PAK_*: each controller's accessory (game.pak, game.pak2...) */
    char pak_file[4][PC_CFG_PATH];  /* each controller's pak image (game.pak_file, game.pak2_file...) */
    /* game.saves (docs/port/SAVES.md): PC_CFG_SAVES_*, each controller's pak kept as the image or as a folder */
    int saves[4];                   /* game.saves, game.saves2... */
    char saves_dir[4][PC_CFG_PATH]; /* game.saves_dir, game.saves2_dir...: the folders */
    /* [video] */
    int width, height;              /* window size, pixels; 0: auto (two thirds of the desktop) */
    char position[32];              /* where the window opens: "center" or "X,Y" */
    int remember_window;            /* save the window's size and place at exit */
    int fullscreen;                 /* start in fullscreen */
    int fullscreen_mode;            /* 0 desktop (borderless), 1 exclusive (PC_GFX_FULLSCREEN_*) */
    char resolution[16];            /* "desktop", a preset ("1620p") or "WxH": pc_settings_resolution */
    int vsync;
    int frame_rate;                 /* frames shown a second: 0 the game's own, -1 the display's (RUSH_GFX_FRAME_RATE) */
    int scale;                      /* internal resolution multiple (RUSH_GFX_SCALE); 0: auto, the window's whole
                                     * multiple; -1: native, the window's own pixels */
    int api;                        /* PC_CFG_API_* (RUSH_GFX_API) */
    double widescreen;              /* display aspect the 4:3 frame widens to: 0 off, -1 the window's (auto) */
    int fill;                       /* the picture's rows fill the window's height: no bands (RUSH_GFX_FILL) */
    int pause_on_focus_loss;
    char screenshot_dir[PC_CFG_PATH];   /* empty: the working directory */
    int textures;                   /* 0 original, 1 upscaled, 2 pack (RUSH_GFX_TEXTURES; docs/port/TEXTURES.md) */
    char texture_pack[PC_CFG_PATH];     /* the pack's folder; a name alone is in the data folder */
    char texture_dump[PC_CFG_PATH];     /* where every texture drawn is written as PNG; empty: off */
    int hud;                        /* the race HUD in widescreen: 1 edges, 0 centered (PC_GFX_HUD_*; RUSH_GFX_HUD) */
    /* draw distance, detail and fog (docs/port/DETAIL.md): PC_DRAW_*, PC_DETAIL_*, PC_FOG_* (pc_port.h) */
    int draw_distance;              /* RUSH_GFX_DRAW_DISTANCE: how far the game's distance limits reach */
    int detail;                     /* the most detailed model of each thing drawn */
    int fog;                        /* RUSH_GFX_FOG */
    int texture_lod;                /* RUSH_GFX_TEXTURE_LOD: PC_TEXLOD_*, the mip level a pixel shows */
    /* [audio] */
    char audio[16];                 /* auto, sdl, alsa, null, wav (RUSH2049_AUDIO) */
    int volume;                     /* master volume, 0-100 */
    int audio_latency;              /* ms of sound the output keeps queued at least; 0: from the device */
    /* [audio] voices and engines (docs/port/AUDIO.md 10): indices into their choices; pc_settings_voices and
     * pc_settings_engines give the values */
    int voices;                     /* original (32), 48, 64, 96: MusyX's voices */
    int engines;                    /* original, nearest2 ... nearest8, all: which drones' engines play */
    /* [input] */
    int keyboard_player;            /* 1-4, 0: no keyboard */
    int first_controller_player;    /* controllers fill players from here, in the order they are plugged in */
    double deadzone;                /* radial, for sticks; also triggers */
    double sensitivity;             /* the stick's travel past the dead zone is multiplied by this (0.5-2) */
    double axis_threshold;          /* a trigger or a stick axis bound to a button presses it past this */
    int stick_range;                /* N64 stick value at full tilt, left/right/up/down */
    int stick_diagonal;             /* each axis at full diagonal tilt (the octagonal gate); 0: a round gate */
    int keyboard_steering;          /* PC_STEER_*: keys and buttons on the stick at full tilt at once, or a ramp */
    int keyboard_steer_time;        /* PC_STEER_SMOOTH: ms from the centre to full tilt (0-1000); back in a third */
    double rumble;                  /* rumble strength 0-1 */
    /* [keyboard], [controller], [hotkeys]: comma-separated names, empty for none */
    char key[PC_NUM_CONTROLS][PC_CFG_BIND];
    int analog;                     /* PC_CFG_STICK_*: which controller stick is the N64 stick */
    char pad[PC_NUM_CONTROLS][PC_CFG_BIND];
    char pad_menu[PC_CFG_BIND];     /* [controller] menu: the buttons that open the settings overlay */
    char hotkey[PC_NUM_HOTKEYS][PC_CFG_BIND];
    /* [net]: online play (docs/port/NETPLAY.md); hosting and joining are never settings (--net-host, --net-join) */
    int net_port;                   /* the session's UDP port */
    int net_delay;                  /* the input delay in retraces, 1-30; 0: auto */
    char net_code[64];              /* the join code; "" for none */
    char net_recent[PC_CFG_PATH];   /* the hosts joined last, the last first (the F1 Online page's; pc_online.h) */
    int net_view;                   /* PC_GFX_VIEW_*: what an online race shows on this PC (own: its player's view) */
    int net_rejoin;                 /* the rejoin window, seconds (30-3600); 0: off (a 15 s silence ends the session) */
    int net_recover;                /* a desync: the guest rebuilds by replay and the session goes on */
    int net_keep;                   /* the sessions netplay/ keeps, the newest (1-1000); 0: all (NETPLAY.md 17) */
    int net_rumble;                 /* this player's controller holds a rumble pak in a session, not its pak */
    int net_upnp;                   /* hosting: this PC's router opens the port (UPnP, NAT-PMP, PCP); off by default */

    /* Not in the file: where it was read from ("" for none) and its directory; where the settings overlay saves
     * (pc_settings_resolve: the file read, the --config path, or the located one; "" for nowhere). */
    char source[PC_CFG_PATH];
    char source_dir[PC_CFG_PATH];
    char save_path[PC_CFG_PATH];
} PcSettings;

/* The settings the game reads (docs/port/LOCKSTEP.md 3, 7; NETPLAY.md 1.5): what a replay file must match, what a
 * session's guest adopts from the host (adopt 1), what the session sets itself (0: the accessories), and what stays
 * each PC's own in a session (-1: audio.latency, which a session's audio keeps out of the game, AUDIO.md 11). One
 * list for the replay header, the guest's adoption and the overlay's locks. pc_settings_session_rule: PC_SESSION_OWN
 * for a setting each PC keeps (one not listed too), SETS or HOSTS for one the overlay locks in a session, FIXED for one
 * each PC keeps that the session reads when it starts (net.rejoin, net.recover, net.rumble): locked too. */
typedef struct {
    const char *section, *key;
    int adopt;
} PcMustMatch;
extern const PcMustMatch pc_must_match[];
extern const int pc_must_match_count;
enum { PC_SESSION_OWN = 0, PC_SESSION_SETS = 1, PC_SESSION_HOSTS = 2, PC_SESSION_FIXED = 3 };
int pc_settings_session_rule(const char *section, const char *key);

/* [game] cars at run time (race_cars.c): the setting the game latches at each race's start (pc_race_cars_begin,
 * pc_game_types.h). main sets it at start and the settings overlay when it changes; 0 (original) is the N64's 6, and
 * anything outside 6 to PC_MAX_LINKS is taken to the nearest end. */
void pc_race_cars_set(int cars);

/* game.memory with game.expansion_pak (docs/port/MEMORY.md 1): a size gives the game that much memory, and it takes its
 * Expansion Pak features from any size past 4 MiB, as a console with that memory would, so expansion_pak = off has no
 * effect then. 1 and a message in why for that combination (pc_settings_resolve prints it), else 0. */
int pc_settings_memory_conflict(const PcSettings *s, char *why, size_t why_size);
/* video.draw_distance, video.detail and video.fog as the game reads them (detail.c): PC_DRAW_*, PC_DETAIL_*, PC_FOG_*
 * (pc_detail.h; the game reads fog only for the sky dome, the renderer has its own). main
 * sets them at start and the settings overlay when they change; the game takes them from its next frame. The
 * renderer's part (the depth range, the fog) is pc_gfx_set_draw_distance and pc_gfx_set_fog. pc_scene_report: the
 * exit's scene line, with the renderer's tasks and triangles (pc_gfx_scene_stats). */
void pc_detail_set(int draw_distance, int detail, int fog);
void pc_scene_report(uint32_t tasks, uint64_t triangles, uint32_t most);
/* [game] frame_rate at run time (game_fps.c, docs/port/FRAMERATE.md 7): 0 original or 60. main sets it at start and
 * the settings overlay when it changes; the game latches it at each change of its state (pc_game_fps_begin,
 * pc_game_fps.h). pc_game_fps_setting is the setting itself, unlatched. */
void pc_game_fps_set(int fps);
int pc_game_fps_setting(void);
/* What the overlay's About page says of it: the setting, and at 60 how many of its frames the game kept in the last
 * race (docs/port/FRAMERATE.md 7.7: under 0.9 a retrace over 4 s, the host can't keep 60 there). */
void pc_game_fps_describe(char *out, size_t size);

/* [mods]: whether a mod's folder is listed on (1), off (0), or not listed (-1, which is on: docs/port/MODS.md). */
int pc_settings_mod_on(const PcSettings *s, const char *folder);
/* [audio] voices and [audio] engines at run time (voices.c, docs/port/AUDIO.md 10). main sets the voices at start,
 * before the game threads run (32 for original; 48, 64, 96), and never again: MusyX is set up once a run. The engines
 * (PC_ENGINES_*, pc_game_types.h: 0 original, -1 all, N the nearest N) apply from the sound manager's next retrace;
 * the overlay sets them when they change. pc_snd_counters: the counters the game's sound code fills (PC_SND_*) and
 * the message pool's high-water mark; pc_engines_on: the drones whose engines played at the last pass, a bit a car
 * (0 at original). */
void pc_voices_set(int voices);
int pc_settings_voices(const PcSettings *s);    /* 32, 48, 64 or 96 */
int pc_settings_engines(const PcSettings *s);   /* PC_ENGINES_*: 0 original, -1 all, else N */
void pc_engines_set(int mode);
void pc_snd_counters(unsigned long long out[], int *msg_most);
int pc_snd_most_of(int what);   /* PC_SND_MOST_*: the fx tags and emitters in use at most */
unsigned pc_engines_on(void);
/* The drones' engine layers that had an emitter at the last emitter pass, those whose tags played, the drones of
 * those (a bit a car), and the most layers that played at one pass: any mode, original too. */
void pc_engines_heard(int *layers, int *playing, unsigned *cars, int *most);

/* video.resolution's text: "desktop" (0 x 0), 720p, 1080p, 1440p, 1620p (2880 x 1620), 2160p, or WxH. -1 if none. */
int pc_settings_resolution(const char *text, int *w, int *h);

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
 * RUSH_GFX_WIDESCREEN, RUSH_GFX_FRAME_RATE, RUSH_GFX_TEXTURES, RUSH_GFX_HUD, RUSH_GFX_DRAW_DISTANCE, RUSH_GFX_FOG.
 * Bad values are reported and ignored.
 * Returns how many were bad. */
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
 * $XDG_CONFIG_HOME/rush2049pc/rush2049pc.ini (~/.config/... without XDG_CONFIG_HOME; %APPDATA%\rush2049pc on
 * Windows). *exists says whether that file exists. -1 when there is no place at all (no exe_dir file, no
 * XDG_CONFIG_HOME and no HOME; no APPDATA). */
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

/* Presets (port/config/presets.c, docs/port/FRONTEND.md "Presets"): several settings in one step, not a setting of
 * their own. n64 is the defaults of every setting the others set; enhanced the recommended PC ones; maximum enhanced
 * with the whole track, no fog and upscaled textures. The settings overlay's Presets page and --preset NAME set them.
 * pc_presets: the table and its size; pc_preset_find: by name in any case, NULL for none; pc_preset_in_effect: 1
 * when every setting the preset sets has its value in s. */
typedef struct {
    const char *name, *label, *desc;   /* --preset's name; the overlay's item and its footer line */
    const PcSettingValue *values;
    int n;
} PcPreset;
int pc_presets(const PcPreset **out);
const PcPreset *pc_preset_find(const char *name);
int pc_preset_in_effect(const PcSettings *s, const PcPreset *p);

#ifdef __cplusplus
}
#endif

#endif
