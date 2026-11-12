/* port/include/pc_port.h: the interfaces between the parts of the PC layer (docs/PORT_PLAN.md).
 *
 *   port/ultra   libultra replacement, ROM, memory, threads, main loop (owns main())
 *   port/data    initial values of the game's data, read from the ROM by declared type
 *   port/gfx     F3DEX2 display-list interpreter and renderer
 *   port/audio   audio command-list interpreter and output
 *   port/assets  the game's files from the ROM, converted to the host's byte order
 *   port/input   keyboard, game controllers, scripted presses (port/config: rush2049pc.ini)
 *
 * PC-layer code only: game C never includes this (it carries its own declarations). Each part implements its
 * functions; port/ultra links weak no-op defaults for any part that isn't built yet. */
#ifndef PC_PORT_H
#define PC_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "pc_n64_map.h"   /* the N64's memory map: the RDRAM window's constants (PC_RDRAM_BASE ...) */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- ROM and memory (port/ultra) */

/* The cartridge image in big-endian (.z64) byte order, loaded and checked at startup. */
const uint8_t *pc_rom_bytes(void);
size_t pc_rom_size(void);

/* The N64's RDRAM as the game addresses it outside its own symbols: the RDRAM window, PC_RDRAM_BASE to
 * PC_RDRAM_BASE + pc_rdram_size() in the N64's KSEG0 (its constants and why: pc_n64_map.h, the N64's memory map). */
void *pc_n64_ptr(uint32_t n64_addr);      /* K0/K1/physical address -> host pointer (NULL if outside RDRAM) */
uint32_t pc_n64_addr(const void *host);   /* host pointer into RDRAM -> K0 address (0 if outside) */
uint32_t pc_rdram_size(void);             /* the window's size: PC_RDRAM_SIZE until pc_mem_set_size */
/* port/ultra/heap.c: whether p lies in the game heap's memory (the main heap's, which holds the others), once the game
 * has made it. */
int pc_heap_holds(const void *p);
/* port/ultra/rcpaddr.c (game C reaches them through pc_prelude.h): a K0 address in the window past 16 MiB, and the
 * display-list relocation's rule, the N64's `((w1 + base) & 0xFFFFFF) | segment` but for the K0 address there. */
int pc_rdram_k0_high(uint32_t addr);
uint32_t pc_rcp_reloc(uint32_t w1, uint32_t base, int32_t seg);

/* Ranges that hold raw big-endian bytes from the ROM (asset buffers filled by DMA). The renderer and the
 * audio interpreter read vertices, matrices, 16-bit texels and command lists in N64 byte order there. */
void pc_mem_register_be(const void *start, size_t size);
void pc_mem_unregister_be(const void *start);
int pc_mem_is_be(const void *p);

/* ---------------------------------------------------------------- paced mode (port/ultra/pace.c) */

/* Paced mode (docs/port/ULTRA.md 10, NETPLAY.md 1.2): the N64 clock is virtual, as with --fast, so the game takes
 * --fast's course to the bit, and it moves to each deadline only once the wall clock has reached that deadline's
 * wall time, so the game plays in real time. What the wall clock does (a slow host, a hold, a vsync swap, these
 * calls) changes when the N64 side moves, never what it does. A mode a session turns on (--paced, RUSH_PACED=1),
 * not a setting.
 *
 * The wall clock here is pc_pace_wall_ns's: monotonic ns since the program started (CLOCK_MONOTONIC;
 * QueryPerformanceCounter on Windows). These calls are thread-safe (the network layer's own thread). */
void pc_time_set_paced(int on);       /* before the boot: paced mode, and with it the virtual clock */
int pc_time_paced(void);
/* An online rejoin's replay (docs/port/NETPLAY.md 15), main thread, at a retrace: on, the N64 clock runs free (as
 * --fast), graphics tasks aren't drawn, nothing is presented and the sound card gets nothing (the tap held), none of
 * which the game sees; off, the mode before it comes back (paced: from the N64 time now, due now). */
void pc_catchup_set(int on);
int pc_catchup(void);
uint64_t pc_pace_wall_ns(void);
/* The session's start, agreed with the peer: N64 time 0 due at this wall time, so retrace N at wall_ns + N/60 s. In
 * the future, the game waits for it at its first deadline; set before pc_ultra_run, or at any time (a jump). */
void pc_pace_set_start(uint64_t wall_ns);
/* Time sync. Once: every deadline from now on ns later (+) or sooner (-). Lasting: every retrace that much longer
 * (+) or shorter (-) in wall time, until changed, capped at +-1 ms a retrace; returns what it set.
 *
 * port/net's clock contract (net.h net_pace_adjust_ns) maps onto these directly: deadline(0) = net_start_time ->
 * pc_pace_set_start (both on pc_real_ns's clock; the session's now_ns is pc_real_ns); each retrace, after net_pads gave
 * its pads, pc_pace_adjust_ns(net_pace_adjust_ns(s)) (0 .. 1 ms: the interval before the next retrace); and the
 * re-base after a stall the loop does itself: when the pads gate held retrace n and its pads came more than half a
 * retrace after n's wall time, n is taken as due at their arrival and n + 1 a retrace (and the adjustment) after it,
 * never catching up; a smaller lateness stays on the grid. */
void pc_pace_shift_ns(int64_t ns);
int64_t pc_pace_adjust_ns(int64_t per_retrace);
typedef struct {
    int paced;                 /* paced mode is on */
    uint32_t retrace;          /* the last retrace that went out (pc_frame) */
    uint64_t retrace_due_ns;   /* the wall time it was due */
    uint64_t retrace_sent_ns;  /* the wall time the clock moved to it (later than due by a lag being caught up) */
    uint64_t next_due_ns;      /* the wall time the next retrace is due, as the map stands now */
    int64_t lag_ns;            /* how late the clock's last move came after its wall time (0: on time) */
    int64_t adjust_ns;         /* pc_pace_adjust_ns's */
    uint32_t holds, rebases;   /* holds the map moved on for; lags of over two retraces re-based */
    uint32_t stalls;           /* the pads gate's waits re-based (pads over half a retrace late) */
} PcPaceInfo;
void pc_pace_info(PcPaceInfo *out);

/* ---------------------------------------------------------------- game data (port/data) */

/* Fill the boot and game images' initialized data from the ROM. 0 on success. */
int pc_data_init(const uint8_t *rom, size_t rom_size);
/* The window when it isn't at host PC_RDRAM_BASE (port/data/pc_data.h, docs/port/NOFIXED.md 5), before pc_data_init. */
void pc_data_set_window(void *(*n64_to_host)(uint32_t n64));

/* An overlay "load": both runtime images share N64_OVERLAY_BASE on the N64; natively both are linked in, and a load
 * resets that image's data to its initial values and zeroes its BSS. */
enum { PC_OVERLAY_A = 0, PC_OVERLAY_B = 1 };
void pc_data_overlay_load(int overlay);

/* ---------------------------------------------------------------- the game's files (port/assets) */

/* _ReadWave, after a wave's file is in its heap block and before SetupWaveModel relocates it: convert it to the
 * host's byte order by format (docs/port/ASSETS.md §5). size is the block's size. */
void pc_wave_loaded(int wave, void *buf, size_t size);
/* The wave's block is freed or moves: drop its big-endian registrations. */
void pc_wave_released(int wave, void *buf);

/* ---------------------------------------------------------------- graphics (port/gfx), host main thread */

/* Display-list command words on the PC: Gfx (and Acmd) keep w0 as 32 bits and widen w1 to hold a host
 * pointer. 16 bytes per command. Lists in a pc_mem_register_be range are N64 form instead: 8 bytes, big-endian,
 * 32-bit (segmented or K0) addresses. */
typedef struct {
    uint32_t w0;
    uintptr_t w1;
} PcGfxWords;

int pc_gfx_init(int headless, int window_width, int window_height);   /* 0 on success */
void pc_gfx_run_dl(const void *dl);          /* one graphics task: OSTask.t.data_ptr */
void pc_gfx_present(const void *framebuffer);   /* osViSwapBuffer: show the frame rendered into it */
int pc_gfx_poll_events(void);                /* window and input events; nonzero when the user quits */
int pc_gfx_window_closed(void);              /* that quit was the window's close (or the system's), not a key */
/* The window's events for port/input: pc_gfx_poll_events passes each one (an SDL_Event *) to hook before its
 * own handling, and a nonzero return quits. NULL removes it. 0 when there is an SDL window, -1 otherwise
 * (headless, or no SDL2), and the hook is then never called. */
int pc_gfx_set_event_hook(int (*hook)(const void *sdl_event));
int pc_gfx_save_screenshot(const char *path);   /* last presented frame as PNG; 0 on success */
/* The window as last drawn, the UI over the frame included, as a PNG (tests: headless, the window's image; with a
 * window, its back buffer, which a swap may have emptied). 0 on success. */
int pc_gfx_save_window(const char *path);
void pc_gfx_shutdown(void);

/* The window and the display (docs/port/GFX.md "Window and display"). Sizes are pixels (the drawable's, at any
 * display scaling). pc_gfx_init's window size 0 x 0 is two thirds of the desktop (headless: 320 x 240, the N64's
 * frame). Without a window these keep their values; pc_gfx_set_window_pixels then sets the headless size. */
enum { PC_GFX_FULLSCREEN_DESKTOP = 0, PC_GFX_FULLSCREEN_EXCLUSIVE = 1 };   /* video.fullscreen_mode's order */
void pc_gfx_set_fullscreen_mode(int mode);       /* what fullscreen means; applies at once when in fullscreen */
/* video.resolution, 0 x 0 for the desktop's: exclusive fullscreen's display mode (the closest one), and the size
 * desktop fullscreen draws the frame at (fitted to the screen). */
void pc_gfx_set_resolution(int w, int h);
int pc_gfx_set_fullscreen(int on);               /* 0 on success */
int pc_gfx_toggle_fullscreen(void);              /* Alt+Enter, the fullscreen hotkey */
int pc_gfx_is_fullscreen(void);                  /* -1 without a window */
int pc_gfx_set_vsync(int on);                    /* 0 on success */
void pc_gfx_set_window_pixels(int w, int h);     /* the window's size (applies after fullscreen when in it) */
void pc_gfx_set_window_position(int x, int y);   /* before pc_gfx_init: where the window opens, if on a display */
/* The window's last place and size out of fullscreen (video.remember_window): x, y in the desktop's coordinates,
 * w x h pixels. -1 without a window. */
int pc_gfx_window_geometry(int *x, int *y, int *w, int *h);

/* A high frame rate (video.frame_rate; docs/port/GFX.md "High frame rate"). 0, the default, is the game's own: each
 * frame is shown when the VI swaps it in (pc_gfx_present). Otherwise the main loop shows a frame this many times a
 * second (pc_gfx_present_tick), drawn between the game's last two; -1 is the display's refresh rate. The game's
 * timing doesn't change: pc_gfx_present still only marks when each frame is swapped in, and screenshots still
 * read the game's frames. */
void pc_gfx_set_frame_rate(int rate);
int pc_gfx_frame_rate(void);                 /* frames shown a second (-1 resolved), 0 for the game's own */
/* video.frame_rate = auto (docs/port/FRAMERATE.md 7.9): the main loop gives the game's rate, vsync and the display's
 * whole hertz at each pass (pc_gfx_auto_frame_rate), and the rate follows pc_gfx_auto_rate's choice for them, said
 * in the log at the first and at each change (a window moved to another display). */
#define PC_GFX_RATE_AUTO (-2)
int pc_gfx_frame_rate_auto(void);
int pc_gfx_auto_rate(int game_rate, int vsync, int hz, const char **why);
void pc_gfx_auto_frame_rate(int game_rate, int vsync, int hz);
void pc_gfx_frame_rate_describe(char *out, size_t n);   /* auto's choice: "auto: 144 Hz, frames between" */
double pc_gfx_shown_moment(void);   /* the moment, in retraces, of the last frame shown: the vsync model's evenness */
/* game.frame_rate = 60 with video.frame_rate original and a swap that waits for vsync (docs/port/FRAMERATE.md 7.6):
 * on, the main loop shows the game's own frames paced by the display's vblanks, as at 60 but with none drawn between
 * (pc_gfx_frame_rate is then 60, pc_gfx_own_paced 1); a presented frame's swap at its retrace held the loop up to a
 * refresh, and on a display in step with the retraces the game kept 30 frames a second. */
void pc_gfx_set_own_paced(int on);
int pc_gfx_own_paced(void);
void pc_gfx_retrace(uint32_t vi);            /* each retrace, before the VI's present: its number */
/* Show the frame for this moment, in retraces (pc_gfx_retrace's). mode 0: as the moment says (--fast); 1: on the
 * host's clock, a frame between two isn't drawn while the GPU hasn't finished what it was given; 2: no frame is drawn
 * between two, only the game's are shown (the host is behind). 1 when something new was shown, 0 if not. */
int pc_gfx_present_tick(double vi_time, int mode);
/* A mode bit: the frame is drawn into the window and its swap left to pc_gfx_swap_tick (the main loop times it, and
 * with vsync swaps when the vblank it was drawn for comes). pc_gfx_vsync: 1 when the window's swap waits for vsync;
 * pc_gfx_display_hz: the display's refresh (60 without one). */
#define PC_GFX_TICK_NO_SWAP 4
void pc_gfx_swap_tick(void);
/* 1 when a tick at vi_time would draw a frame between the one shown and the game's next, which isn't drawn yet. */
int pc_gfx_tick_waits(double vi_time);
int pc_gfx_vsync(void);
int pc_gfx_display_hz(void);
/* The display's exact refresh where the platform gives it (docs/port/FRAMERATE.md 2: Windows' display configuration
 * and compositor, X11's XRandR): bit 1, *period_ns is its period; bit 2, a vblank was *vblank_age_ns ago (negative: to
 * come). 0: nothing beyond pc_gfx_display_hz's whole hertz (headless, Wayland). */
int pc_gfx_display_timing(double *period_ns, int64_t *vblank_age_ns);
/* 1 when the game has swapped in a frame the window hasn't shown yet (a high frame rate): a tick the main loop
 * skips to keep the retraces on time still shows it, as the setting off would at its swap. */
int pc_gfx_frame_waiting(void);
/* Textures (docs/port/TEXTURES.md): 0 original, 1 upscaled (3D textures, by the internal scale), 2 pack (replacements
 * from the texture pack folder). From the next frame. pc_gfx_parse_textures: "original", "upscaled", "pack"; *ok = 0
 * for anything else. */
void pc_gfx_set_textures(int mode);
int pc_gfx_parse_textures(const char *text, int *ok);
/* The race HUD in widescreen (docs/port/GFX.md "The HUD at the screen's edges"). PC_GFX_HUD_EDGES (the default) moves
 * the HUD's pieces at the 4:3 frame's sides out by the widening, so they keep their distance to the screen's edges,
 * while race() says a race with one view is on; PC_GFX_HUD_CENTERED keeps all 2D in the 4:3 frame. From the next
 * graphics task. pc_gfx_parse_hud: "edges", "centered"; *ok = 0 for anything else. */
enum { PC_GFX_HUD_CENTERED = 0, PC_GFX_HUD_EDGES = 1 };
void pc_gfx_set_hud(int mode);
int pc_gfx_hud(void);
int pc_gfx_parse_hud(const char *text, int *ok);
void pc_gfx_set_hud_race(int (*race)(void));
/* One view of the split screen alone, across the frame (net.view; port/gfx/gfx_view.c, docs/port/GFX.md "One view of
 * the split screen", docs/port/NETPLAY.md 14). PC_GFX_VIEW_SPLIT (the default) draws the game's split screen;
 * PC_GFX_VIEW_OWN draws player pc_gfx_set_view_player's view alone (1-4; 0, the default, none: split) in a race that
 * views() says has several views: it gives their rectangles in the frame (x0, y0, x1, y1, N64 pixels: the game's
 * scissors), their count, or 0 outside such a race. Render-only: nothing reaches the game. From the next graphics task.
 * pc_gfx_parse_view: "split", "own"; *ok = 0 for anything else. */
enum { PC_GFX_VIEW_SPLIT = 0, PC_GFX_VIEW_OWN = 1 };
void pc_gfx_set_view(int mode);
int pc_gfx_view(void);
int pc_gfx_parse_view(const char *text, int *ok);
void pc_gfx_set_view_player(int player);
void pc_gfx_set_race_views(int (*views)(int rect[][4], int max));
/* Draw distance, detail and fog (video.draw_distance, video.detail, video.fog; docs/port/DETAIL.md). The game's
 * distance limits follow draw_distance (port/config/detail.c, pc_game_types.h); the renderer moves the depth range's
 * far plane out with it and the fog with video.fog (gfx_detail.c), from the next triangle. At original the N64's. */
#include "pc_detail.h"   /* PC_DRAW_*, PC_DETAIL_*, PC_FOG_*, PC_DRAW_FAR_FACTOR, PC_DRAW_MAX_DEPTH */
void pc_gfx_set_draw_distance(int level);   /* PC_DRAW_* */
int pc_gfx_draw_distance(void);
void pc_gfx_set_fog(int mode);              /* PC_FOG_* */
int pc_gfx_fog(void);
int pc_gfx_parse_draw_distance(const char *text, int *ok);   /* "original", "far", "max"; *ok = 0 otherwise */
int pc_gfx_parse_fog(const char *text, int *ok);             /* "original", "far", "off" */
/* video.texture_lod (gfx_detail.c, docs/port/DETAIL.md 2.7): which level of a mip chain (G_TL_LOD) a pixel shows,
 * from the next triangle. PC_TEXLOD_*; at original the N64's. */
void pc_gfx_set_texture_lod(int mode);
int pc_gfx_texture_lod(void);
int pc_gfx_parse_texture_lod(const char *text, int *ok);     /* "original", "sharp", "anisotropic" */
/* What the graphics tasks drew so far (the exit's scene line): tasks, their triangles in all and the most in one. */
void pc_gfx_scene_stats(uint32_t *tasks, uint64_t *triangles, uint32_t *most);
/* Where the HUD moves a point at frame x (N64 pixels of the 4:3 frame), in N64 pixels: what the game's 3D models
 * placed at HUD points (ZOID_WindowTo3D) add to their x. 0 unless edges is in effect. */
int pc_gfx_hud_dx(int x);
/* 1: pc_gfx_hud_dx gives the game 0, whatever the widening (a lockstep session, docs/port/LOCKSTEP.md 6: the models'
 * places are game state, and the two PCs' windows differ). The 2D HUD still moves; the models then sit at the 4:3
 * frame's places. 0: as above (the default). */
void pc_gfx_hud_dx_off(int off);

/* Drawing over the frame, in window pixels (port/gfx/gfx_ui.c), for port/input's settings overlay. While a UI hook
 * is set, each present (and pc_gfx_redraw) calls it with the window's size after the frame is shown and before
 * the swap; it draws with pc_gfx_ui_rect and pc_gfx_ui_text. Screenshots keep the frame without it. NULL removes
 * it. Colors are 0xRRGGBBAA, blended over what is there. */
void pc_gfx_set_ui_hook(void (*draw)(int window_w, int window_h));
void pc_gfx_ui_rect(int x, int y, int w, int h, uint32_t rgba);
/* Printable ASCII in the port's own font (gfx_font.c): 5 x 9 pixels per glyph at scale 1 (7 above the baseline),
 * 6 apart; y is the top of the cell. Returns the width drawn. */
int pc_gfx_ui_text(int x, int y, int scale, uint32_t rgba, const char *text);
#define PC_GFX_FONT_W 5
#define PC_GFX_FONT_H 9
#define PC_GFX_FONT_ADVANCE 6
/* A glyph's rows, top first; bit 4 is the leftmost column. 0 for a character the font lacks. */
int pc_gfx_font_glyph(int c, uint8_t rows[PC_GFX_FONT_H]);
/* Show the last presented frame again, with the UI over it, and swap (the overlay while the game is held). */
void pc_gfx_redraw(void);
/* The GL driver as the log's "gfx:" line names it: GL_VENDOR, GL_RENDERER, GL_VERSION and the shading language's
 * version; "" before pc_gfx_init. For the overlay's About page. */
void pc_gfx_driver(const char **vendor, const char **renderer, const char **version, const char **glsl);
/* Called by pc_gfx_poll_events after the window's events, also without a window: port/input's turn at each
 * retrace and while the game is held. A nonzero return quits. NULL removes it. */
void pc_gfx_set_tick_hook(int (*tick)(void));

/* ---------------------------------------------------------------- audio (port/audio) */

/* Output setup, from main before the game starts. backend: "auto" (SDL2, then ALSA, then null), "sdl",
 * "alsa", "null" or "wav"; dump_wav_path: NULL, or a WAV file that receives every pushed frame
 * (--dump-audio). 0 on success; otherwise output falls back to null. Without it, the first audio call
 * initializes from the environment (port/audio/pc_audio.h). */
int pc_audio_init(const char *backend, const char *dump_wav_path);
void pc_audio_shutdown(void);
/* The master volume, 0-1 (audio.volume / 100), on the output device; 1 leaves the samples as they are. */
void pc_audio_set_volume(double volume);
/* The clock, in seconds, by which the null backend drains the queue: NULL for the host's (the default); --fast
 * gives it the N64 clock. */
void pc_audio_set_clock(double (*now_seconds)(void));
/* One audio task: OSTask.t.data_ptr / data_size, as the game sets them. Runs to completion. The game's
 * microcode is MusyX v1, not Acmd: data_ptr is an array of subframe descriptors and data_size is how many
 * (not bytes); each writes 192 stereo frames into an AI buffer (port/audio/musyx_abi.h). */
void pc_audio_run_task(const void *cmdlist, size_t data_size);
/* osAiSetFrequency / osAiSetNextBuffer / osAiGetLength. Samples are the game's interleaved stereo s16 in
 * native byte order. */
void pc_audio_set_frequency(uint32_t hz);
void pc_audio_push(const int16_t *samples, size_t stereo_frames);
size_t pc_audio_queued_frames(void);
/* The AI's pacing: the game frames to keep queued (from the device's pulls; it grows after an underrun), and a
 * function the device's thread calls after each pull, so the AI can ask for buffers without the main thread. */
size_t pc_audio_target_frames(void);
void pc_audio_set_pull_hook(void (*hook)(void));
/* The least the queue keeps, in milliseconds (audio.latency); 0: from the device. */
void pc_audio_set_latency(unsigned ms);
/* The main loop holds the game: the AI asks for no buffers, and the silence isn't counted as underruns. */
void pc_audio_set_paused(int paused);
/* Paced mode (docs/port/AUDIO.md 11), before pc_audio_init: the queue the AI keeps is the null output's on the
 * clock pc_audio_set_clock gives (the N64's), as with --fast and the null backend, whatever the backend; a device is
 * fed from a tap of it, a FIFO whose own converter is nudged to keep its level. audio.latency is then the FIFO's. */
void pc_audio_set_paced(int on);
/* Paced mode's tap held (a rejoin's replay): the device gets silence and the FIFO nothing; the game's queue is as it
 * was. Released, the FIFO fills to its level again before it plays. */
void pc_audio_tap_hold(int on);
/* The output's counters (underruns, the queue's level, buffers late for their render...) go to the log at exit;
 * with this, also every `seconds` while the game plays (-v: 10). pc_audio_periodic is the main loop's, about once a
 * second. */
void pc_audio_set_report_every(unsigned seconds);
void pc_audio_periodic(void);

/* ---------------------------------------------------------------- input (port/input) */

typedef struct {
    uint16_t buttons;   /* N64 button bits (CONT_A 0x8000 ... as libultra) */
    int8_t stick_x;
    int8_t stick_y;
    int connected;
    int rumble;         /* its controller has a rumble motor (an accessory set to auto is then a rumble pak) */
} PcPad;
void pc_input_poll(PcPad pads[4]);

#ifdef __cplusplus
}
#endif

#endif
