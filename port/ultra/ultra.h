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
void pc_os_wait_main(uint64_t deadline_ns);
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
extern uint32_t pc_rcp_gfx_tasks, pc_rcp_audio_tasks;     /* counts, for status lines */
extern int pc_rcp_stop_at_gfx;                            /* --stop-at-gfx: exit at the first graphics task */

/* ---------------------------------------------------------------- pi.c: ROM */
int pc_rom_load(const char *path, int check_sha1);   /* 0 on success; prints why not */
int pc_rom_blank(void);                              /* --no-rom: a zeroed 12 MiB cartridge */
int pc_rom_copy(uint32_t rom_offset, void *dst, uint32_t size);   /* 0 if inside the ROM */
uint32_t pc_rom_word(uint32_t rom_offset);           /* big-endian word, 0 past the end */
void pc_sha1(const void *data, size_t size, uint8_t out[20]);

/* ---------------------------------------------------------------- si.c: PIF, pads, controller pak */
int pc_si_init(const char *pak_path, int pak_kind);  /* pak_kind: PC_PAK_* */
enum { PC_PAK_NONE = 0, PC_PAK_MEMORY = 1, PC_PAK_RUMBLE = 2 };
void pc_si_set_pads(const PcPad pads[4]);            /* main thread, each retrace */
/* The rumble pak's motor on a channel: 1 while it is on, or if it was turned on since the last call (a pulse
 * between two calls counts). For port/input, which passes it to the controller. */
int pc_si_motor(int channel);
void pc_si_flush(void);                              /* write the .mpk if it changed */

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
