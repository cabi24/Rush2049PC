/* port/include/pc_crossings.h: the crossing counter's and the arena guard's hooks (docs/port/NATIVE.md 9).
 *
 * A crossing is a place where the game or the PC layer turns a 32-bit N64 address into a host pointer, or a host
 * pointer into one: the game C's translations (PC_N64P, PC_N64A, PC_N64W, the handle slots, the StatLL lists' 32-bit
 * links), port/ultra's pc_n64_ptr and pc_n64_addr, and the renderer's and the audio interpreter's reads of values below
 * 4 GiB as RDRAM addresses. Wave 3 takes them away one by one; a build with RUSH_COUNT_CROSSINGS
 * (cmake -DRUSH_COUNT_CROSSINGS=ON) counts them, by site and by where the address points, and writes the counts at exit
 * (port/ultra/crossings.c; tools/port/crossings.py runs the synthetic paths and reports them). The same build has the
 * arena guard: RUSH2049_WINDOW_GUARD=arenas maps the game's data arenas in the RDRAM window without access, so
 * whatever still reaches that data by its N64 address faults.
 *
 * Without the option every hook below is nothing, and the default build compiles exactly as it did. Included by
 * pc_prelude.h (the game C) and by the PC layer's files with a hook (port/ultra/memory.c, port/gfx/gfx_rsp.c,
 * port/audio/musyx.c, port/data/pc_data.c, port/main.c); no system headers, since the game C includes it too. */
#ifndef PC_CROSSINGS_H
#define PC_CROSSINGS_H

/* What crossed. The dump and the report name each kind as crossings.c's table does (pc_cross_kind_name). */
enum pc_cross_kind {
    PC_CROSS_N64P = 1,       /* game C, by source line: PC_N64P(a), an N64 address taken as a host pointer */
    PC_CROSS_N64A,           /* game C: PC_N64A(p), a host pointer's N64 address (the heap's arithmetic) */
    PC_CROSS_N64W,           /* game C: PC_N64W(p), a pointer stored in a 32-bit field */
    PC_CROSS_SLOT_N64,       /* game C: PC_SLOT_N64(s), a heap handle's slot read as an N64 address (gone, step 6) */
    PC_CROSS_SLOT_SET,       /* game C: PC_SLOT_SET(s, a), an N64 address stored in a handle's slot (gone, step 6) */
    PC_CROSS_STATLL_GET,     /* game C: a StatLL list's 32-bit link read (the level-object nodes' pc_statll_link,
                              * gone with NATIVE.md step 3: every list links by host pointers; the kind stays, 0) */
    PC_CROSS_STATLL_SET,     /* game C: one written (pc_statll_set_link, gone with step 3) */
    PC_CROSS_N64_PTR,        /* by caller: pc_n64_ptr(a), an N64 address made a host pointer */
    PC_CROSS_N64_ADDR,       /* by caller: pc_n64_addr(p), a host pointer's N64 address */
    PC_CROSS_VIRT_TO_PHYS,   /* by caller: osVirtualToPhysical(p), the colour and depth images' addresses. None since
                              * NATIVE.md step 4, which took the function away; the kind stays so that a report
                              * compares with the baseline (NATIVE.md 9.4) line by line */
    PC_CROSS_GFX_ROM_LIST,   /* by caller: the renderer reads an address below 4 GiB from a list in the ROM's form */
    PC_CROSS_GFX_LIST,       /* by caller: the same from a native list (one the game or the PC built) */
    PC_CROSS_AUDIO_N64PTR,   /* by caller: the audio interpreter reads an N64 address (n64ptr: an N64-form record's
                              * field, or one of resolve's below) */
    PC_CROSS_AUDIO_FIELD,    /* by caller: a native audio record's pointer field holding an N64 address (resolve) */
    PC_CROSS_KINDS
};

#ifdef RUSH_COUNT_CROSSINGS
/* crossings.c. Weak: a test program that links the game C or a part of the PC layer without port/ultra counts nothing.
 * pc_cross_at counts a crossing at a source line (the game C's macros), pc_cross_by one by the code that called the
 * function counting it (a return address, which the report turns into a function and a line). value is the N64
 * address, or the host pointer for the kinds that go from a pointer to an address. */
void pc_cross_at(int kind, unsigned long long value, const char *file, int line) __attribute__((weak));
void pc_cross_by(int kind, unsigned long long value, const void *caller) __attribute__((weak));
/* The arena guard: a graphics task starts (its pages close again: one frame's first touches are what it counts), and
 * the guard paused while the data loader fills an image (pc_cross_guard_pause returns the token pc_cross_guard_resume
 * takes). */
void pc_cross_frame(void) __attribute__((weak));
int pc_cross_guard_pause(void) __attribute__((weak));
void pc_cross_guard_resume(int *token) __attribute__((weak));
/* main.c: the guard starts once the game's data is loaded; the counts are written before the program exits. */
void pc_crossings_start(void) __attribute__((weak));
void pc_crossings_finish(void) __attribute__((weak));

/* In a function of the PC layer that reads N64 addresses: count one by this function's caller. The counting build is
 * compiled with -fno-inline (CMakeLists.txt), so the caller is the code that called this function, not its caller's. */
#define PC_CROSS_CALLER(kind, value) \
    (pc_cross_by ? pc_cross_by((kind), (unsigned long long)(value), __builtin_return_address(0)) : (void)0)
/* The renderer, at the start of each graphics task. */
#define PC_CROSS_FRAME() (pc_cross_frame ? pc_cross_frame() : (void)0)
/* The data loader: the guard paused until the end of the enclosing block, however the block is left (the variable's
 * cleanup resumes it). */
static __inline__ void pc_cross_guard_resume_at_exit(int *token)
{
    if (pc_cross_guard_resume)
        pc_cross_guard_resume(token);
}
#define PC_CROSS_GUARD_PAUSED() \
    int pc_cross_paused_ __attribute__((cleanup(pc_cross_guard_resume_at_exit), unused)) = \
        pc_cross_guard_pause ? pc_cross_guard_pause() : 0
#define PC_CROSSINGS_START() (pc_crossings_start ? pc_crossings_start() : (void)0)
#define PC_CROSSINGS_FINISH() (pc_crossings_finish ? pc_crossings_finish() : (void)0)
#else
#define PC_CROSS_CALLER(kind, value) ((void)0)
#define PC_CROSS_FRAME() ((void)0)
#define PC_CROSS_GUARD_PAUSED() ((void)0)
#define PC_CROSSINGS_START() ((void)0)
#define PC_CROSSINGS_FINISH() ((void)0)
#endif

#endif
