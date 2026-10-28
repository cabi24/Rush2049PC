/* port/ultra/ultra.h: internals shared by the libultra replacement's files (docs/port/ULTRA.md).
 *
 * The game's own declarations (include/PR, include/types.h) can't be included here: types.h redefines
 * uintptr_t as 32 bits. The few libultra types this layer reads are mirrored below with the layout the game
 * gets when it is compiled natively (8-byte pointers). The game's OSThread, OSMesgQueue and OSTimer objects
 * are never written: they are N64-sized in the BSS arenas, so this layer keeps its state in side tables keyed
 * by their addresses. */
#ifndef PC_ULTRA_H
#define PC_ULTRA_H

#include <stddef.h>
#include <stdint.h>

#include "pc_port.h"

typedef void *OSMesg;

/* libultra constants the game passes. */
enum {
    OS_MESG_NOBLOCK = 0,
    OS_MESG_BLOCK = 1,
    OS_READ = 0,
    OS_WRITE = 1,
};
enum {   /* osSetEventMesg */
    OS_EVENT_SW1 = 0, OS_EVENT_SW2, OS_EVENT_CART, OS_EVENT_COUNTER, OS_EVENT_SP, OS_EVENT_SI, OS_EVENT_AI,
    OS_EVENT_VI, OS_EVENT_PI, OS_EVENT_DP, OS_EVENT_CPU_BREAK, OS_EVENT_SP_BREAK, OS_EVENT_FAULT,
    OS_EVENT_THREADSTATUS, OS_EVENT_PRENMI, OS_NUM_EVENTS
};
enum { M_GFXTASK = 1, M_AUDTASK = 2 };

/* OSTask_t (include/types.h) as the game compiles natively. */
typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t *ucode_boot;
    uint32_t ucode_boot_size;
    uint64_t *ucode;
    uint32_t ucode_size;
    uint64_t *ucode_data;
    uint32_t ucode_data_size;
    uint64_t *dram_stack;
    uint32_t dram_stack_size;
    uint64_t *output_buff;
    uint64_t *output_buff_size;
    uint64_t *data_ptr;
    uint32_t data_size;
    uint64_t *yield_data_ptr;
    uint32_t yield_data_size;
} PcOSTask;

/* N64 time: osGetTime counts at 46.875 MHz (half the CPU clock). */
#define PC_COUNT_HZ 46875000ull
#define PC_VI_HZ 60
uint64_t pc_host_ns(void);              /* monotonic, since startup: the N64 clock (virtual with --fast) */
uint64_t pc_real_ns(void);              /* the host's monotonic clock, since startup, whatever the mode */
void pc_time_set_virtual(int on);       /* --fast, before the boot: the clock moves only by pc_time_advance */
int pc_time_virtual(void);
void pc_time_advance(uint64_t ns);      /* main thread, --fast: the clock jumps to ns (never back) */
uint64_t pc_ticks_now(void);            /* osGetTime */
uint64_t pc_ticks_to_ns(uint64_t ticks);

/* ---------------------------------------------------------------- sched.c: threads, queues, events */

/* One N64 thread runs at a time (the baton). The scheduler lock is internal to sched.c; the other files keep
 * their own locks and never hold one while calling in here. */
/* Interrupt-style send: never blocks, never switches threads; -1 when the queue is full. Any context. */
int pc_os_post(void *mq, OSMesg msg);
/* Send the message registered for an event (osSetEventMesg), if any. Any context. */
void pc_os_event(int event);
/* Start the boot context (the game's main) on its own N64 thread and give it the baton. */
void pc_os_boot(void (*entry)(void *), void *arg, const char *name);
/* Busy-wait yield for game loops that poll memory another thread writes (docs/port/ULTRA.md). */
void pc_os_spin_yield(const char *site);
/* Idle thread: wait until another thread can run. */
void pc_os_idle(void);
/* Main thread: wait for wake-ups (task submitted, timer set) until the deadline (pc_host_ns), unlocked. */
int pc_os_wait_main(uint64_t deadline_ns);   /* 0: --fast, a thread still computes (see sched.c) */
void pc_os_quiet_mark(void);   /* real time: the main thread sends a retrace now */
int pc_os_quiet(void);         /* real time: the threads are done with it; if not, the main wait ends when they are */
void pc_os_wake_main(void);
/* Name of the running N64 thread for logs ("main" on the host main thread). */
const char *pc_os_thread_name(void);
void pc_os_dump_threads(void);   /* to stderr, for diagnostics */
extern int pc_os_trace;          /* --trace-os: log switches and blocking calls */

/* ---------------------------------------------------------------- timer.c */
uint64_t pc_timer_service(uint64_t now_ticks);   /* main thread: fires due timers; next deadline (ticks) or 0 */

/* ---------------------------------------------------------------- vi.c */
void pc_vi_retrace(const void **present_fb);     /* main thread: one retrace; *present_fb set on a swap */

/* ---------------------------------------------------------------- rcp.c: RSP tasks and the AI */
int pc_rcp_service(uint64_t now_ns, uint64_t *next_ns);   /* main thread, unlocked; nonzero if work was done */
int pc_rcp_pending(void);                                  /* RSP tasks queued and not run yet */
void pc_rcp_hold_gfx(int on);                              /* the selftest's: graphics tasks wait in the queue */
extern uint32_t pc_rcp_gfx_tasks, pc_rcp_audio_tasks;     /* counts, for status lines (audio: __atomic) */
extern uint32_t pc_rcp_gfx_yields;                         /* graphics tasks that yielded to an audio task */
extern int pc_rcp_stop_at_gfx;                            /* --stop-at-gfx: exit at the first graphics task */

/* ---------------------------------------------------------------- romcheck.c, pi.c: ROM */
/* The cartridge the port runs: San Francisco Rush 2049 (USA), 12 MiB. */
#define PC_ROM_SHA1 "3f99351d7bb61656614bdb2aa1a90cfe55d1922c"
#define PC_ROM_USA_SIZE 0xC00000u
#define PC_ROM_MAX_SIZE ((size_t)64 << 20)
enum {
    PC_ROM_OK,             /* the USA cartridge */
    PC_ROM_MISSING,        /* no file at the path */
    PC_ROM_UNREADABLE,     /* a folder, no permission, a read error */
    PC_ROM_NOT_ROM,        /* empty, too small or big, no N64 header */
    PC_ROM_ARCHIVE,        /* a .zip, .7z, .rar, .gz... */
    PC_ROM_WRONG_GAME,     /* an N64 ROM of another game */
    PC_ROM_OTHER_VERSION,  /* Rush 2049 of another region or revision */
    PC_ROM_BAD_DUMP        /* the USA header, other contents (or size) */
};
typedef struct PcRomInfo {
    int status;            /* PC_ROM_* */
    char order[4];         /* "z64", "v64" or "n64"; "" when not an N64 ROM */
    char title[21];        /* the header's name, printable ASCII, trimmed */
    char code[5];          /* the header's game code: "NRUE" for the USA cartridge */
    int version;           /* the header's revision byte */
    size_t size;           /* the file's size */
    char sha1[41];         /* of the image in .z64 order; "" when not an N64 ROM */
    char what[240];        /* what the file is, for the player: "it is another N64 game: ..." */
} PcRomInfo;
/* What an image is (romcheck.c): swaps it to .z64 order in place when it is a .v64 or .n64 dump, and fills info.
 * Returns info->status. */
int pc_rom_identify(uint8_t *image, size_t size, PcRomInfo *info);
const char *pc_rom_kind(const uint8_t *head, size_t n);   /* "a ZIP archive"... by the first bytes, or NULL */
/* The file as the cartridge when it is the USA ROM, or (check_sha1 off) any N64 ROM; nothing printed. Returns
 * info->status; the cartridge is loaded for PC_ROM_OK, and with check_sha1 off for the last three. */
int pc_rom_open(const char *path, int check_sha1, PcRomInfo *info);
int pc_rom_load(const char *path, int check_sha1);   /* pc_rom_open, printing why not; 0 when loaded */
int pc_rom_blank(void);                              /* --no-rom: a zeroed 12 MiB cartridge */
int pc_rom_copy(uint32_t rom_offset, void *dst, uint32_t size);   /* 0 if inside the ROM */
uint32_t pc_rom_word(uint32_t rom_offset);           /* big-endian word, 0 past the end */
void pc_sha1(const void *data, size_t size, uint8_t out[20]);

/* ---------------------------------------------------------------- si.c: PIF, pads, controller paks */
/* Each controller's accessory. AUTO: a rumble pak while the port's controller can rumble (PcPad.rumble), else none. */
enum { PC_PAK_NONE = 0, PC_PAK_MEMORY = 1, PC_PAK_RUMBLE = 2, PC_PAK_AUTO = 3 };   /* = port/config's PC_CFG_PAK_* */
int pc_si_init(const char *pak_path, int pak_kind);  /* controller 1's: pc_si_set_pak_file, then pc_si_set_pak */
/* Where channel ch's (0-3) controller pak is kept (NULL: in memory only, a formatted pak), before it is chosen. */
void pc_si_set_pak_file(int ch, const char *path);
/* Channel ch's accessory, PC_PAK_*: before the game starts, or later as a hot swap (the old one out at once, the new
 * one in after the game's next poll of every port has found the port empty). Main thread. A memory pak's file is
 * read when it is first chosen, and written when it comes out. Returns kind, or PC_PAK_NONE when the file isn't a
 * controller pak (logged). */
int pc_si_set_pak(int ch, int kind);
int pc_si_pak(int ch);                     /* what the game finds in the port now: PC_PAK_NONE, MEMORY or RUMBLE */
const char *pc_si_pak_path(int ch);        /* the memory pak's file when that is the port's choice, else NULL */
void pc_si_set_pads(const PcPad pads[4]);            /* main thread, each retrace */
void pc_si_test_pads(const PcPad pads[4]);  /* --selftest: these pads until NULL, whatever the main loop sets */
/* The rumble pak's motor on a channel: 1 while it is on, or if it was turned on since the last call (a pulse
 * between two calls counts). For port/input, which passes it to the controller. */
int pc_si_motor(int channel);
void pc_si_flush(void);                              /* write the .mpk if it changed */
int pc_si_pak_check(char *why, size_t why_size);     /* the pak in memory: pc_pak_image_check (self-test) */

/* ---------------------------------------------------------------- pakfile.c: the controller pak's file */
#define PC_PAK_SIZE 0x8000u
enum { PC_PAK_IMAGE_OK = 0, PC_PAK_IMAGE_N64 = 1, PC_PAK_IMAGE_DAMAGED = 2 };
/* What a .mpk image is: one the game can use, one in N64 byte order (left alone), or one the game's libultra would
 * write over (the wrong size, no good ID block, no good inode table). why gets a sentence ("it is ...") unless OK. */
int pc_pak_image_check(const uint8_t *image, size_t size, char *why, size_t why_size);
void pc_pak_format(uint8_t *image);                  /* a blank formatted pak, PC_PAK_SIZE bytes */
void pc_pak_id_sums(const uint8_t *id, int be, uint16_t *sum, uint16_t *isum);   /* __osIdCheckSum */
int pc_pak_id_valid(const uint8_t *id, int be);
/* Any pak file (each player's), as the game is to see it, into image (PC_PAK_SIZE bytes). PC_PAK_LOAD_FILE: the
 * file's image. PC_PAK_LOAD_NEW: no file yet; image formatted, to be written. PC_PAK_LOAD_N64: the file is in N64
 * byte order; image formatted, and the file is not to be written. PC_PAK_LOAD_NONE: the file can't be used
 * (unreadable, damaged; port/app's saves.c sets such a file aside before the game starts); no pak, and the file is
 * not to be written. why says why for the last two. The game never writes over a file it can't use. */
enum { PC_PAK_LOAD_FILE, PC_PAK_LOAD_NEW, PC_PAK_LOAD_N64, PC_PAK_LOAD_NONE };
int pc_pak_file_load(const char *path, uint8_t *image, char *why, size_t why_size);
/* path written whole or not at all: PATH.tmp written, flushed to the disk, then renamed over path (and, on POSIX,
 * the folder flushed). 0 on success; -1 with errno set, path untouched and no PATH.tmp left. The pak's writes. */
int pc_file_write_atomic(const char *path, const void *data, size_t size);

/* ---------------------------------------------------------------- memory.c */
int pc_mem_init(void);          /* map the RDRAM window; 0 on success */
int pc_mem_fixed(void);         /* the window is at host 0x80000000 */
int pc_mem_fix_absolute(void);  /* first thing in main: GOT slots of absolute N64-address symbols */
int pc_fix_absolute_got(void);  /* the same, without the log (absfix.c, also linked by tests) */
uint8_t *pc_rdram(void);

/* ---------------------------------------------------------------- misc.c */
void pc_os_initialize(void);    /* osInitialize: the low-memory globals */
uint32_t pc_os_mem_size(void);
void pc_os_set_expansion_pak(int on);   /* osMemSize 8 MiB instead of 4 */
void pc_print_backtrace(int skip);
void pc_fault_install(void);       /* SIGSEGV and friends: report the N64 thread and a backtrace */
/* main's part of the end of the process (port/app's log, docs/port/RELEASE.md): pc_on_fault after a fault's report
 * (what: its first line), before the process ends; pc_on_exit before an _exit that ends a run. NULL: nothing. */
extern void (*pc_on_fault)(const char *what);
extern void (*pc_on_exit)(int code);
void pc_fault_thread_init(void);   /* per host thread: an alternate signal stack */

/* ---------------------------------------------------------------- port/input */
int pc_input_add_spec(const char *spec);   /* --press T:BUTTON[@PORT][:DUR],...; 0 on success */
int pc_input_add_file(const char *path);   /* --press-file */
extern int pc_input_script_pak;            /* a press file's `pak on|off`, else -1 */
extern uint32_t pc_input_script_frames;    /* a press file's `frames N` */
struct PcSettings;                         /* port/config/config.h */
/* After pc_gfx_init: live keyboard and controllers when there is an SDL window (0), else scripts only (-1). */
int pc_input_init(const struct PcSettings *settings);
void pc_input_shutdown(void);
int pc_input_paused(void);                 /* the pause key, or focus lost with pause_on_focus_loss */
extern int pc_input_log;                   /* --log-input: print every change of a player's pad */
extern const char *pc_input_title_tag;     /* after the window's title ("Rush2049PC v1.0"); NULL: none */
int pc_input_set_via(const char *spec);      /* --press-via KIND[:SECONDS]; 0 on success */

/* ---------------------------------------------------------------- loop.c */
typedef struct {
    double seconds;          /* stop after this long (0: run until quit) */
    uint32_t frames;         /* stop after this many graphics tasks (0: no limit) */
    int status_every;        /* print a status line every N seconds (0: never) */
    uint32_t screenshot_at;  /* save a screenshot once this many graphics tasks have run (0: never) */
    const char *screenshot;  /* its path */
    /* The game's state word (gGameState), read once per retrace on the host main thread: each change is logged.
     * NULL: none (the self-test). */
    uint32_t (*game_state)(void);
    void (*game_info)(char *out, size_t size);   /* more to log with each change (players, mode...), or NULL */
    uint32_t until_state;        /* stop once the state word has one of these bits... */
    uint32_t until_state_frames; /* ...and this many more graphics tasks have run; a time limit reached first
                                    is a failure (exit 1). 0: no such stop */
} PcRunOptions;
int pc_ultra_run(const PcRunOptions *opt);   /* the host main loop; returns the exit code */
extern uint32_t pc_frame;                    /* retraces since boot */
extern volatile int pc_quit;                 /* set to stop the main loop (exit code pc_quit - 1) */

/* Logging. On mingw-w64 a `printf` format attribute checks Microsoft's formats, but its printf family, and so
 * pc_log, takes C99's (%zu). */
#ifdef __MINGW_PRINTF_FORMAT
#define PC_PRINTF_FORMAT __MINGW_PRINTF_FORMAT
#else
#define PC_PRINTF_FORMAT printf
#endif
void pc_log(const char *fmt, ...) __attribute__((format(PC_PRINTF_FORMAT, 1, 2)));
void pc_fatal(const char *fmt, ...) __attribute__((format(PC_PRINTF_FORMAT, 1, 2), noreturn));
extern int pc_verbose;

#endif
