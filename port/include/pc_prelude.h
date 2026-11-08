/* port/include/pc_prelude.h: force-included first into every game C file (CMakeLists.txt, game_flags). No system
 * headers: each game file carries its own declarations. docs/port/POINTERS.md describes how the game C uses it. */
#ifndef PC_PRELUDE_H
#define PC_PRELUDE_H

/* Pointer-sized integers. */
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTPTR_TYPE__ intptr_t;

/* Words of N64-form data that stays big-endian on PC (display lists, vertices and other payloads loaded
 * from the ROM; see pc_mem_register_be in pc_port.h). */
static __inline__ unsigned int pc_be32_get(const void *p) {
    const unsigned char *b = (const unsigned char *)p;
    return ((unsigned int)b[0] << 24) | ((unsigned int)b[1] << 16) | ((unsigned int)b[2] << 8) | b[3];
}
static __inline__ void pc_be32_set(void *p, unsigned int v) {
    unsigned char *b = (unsigned char *)p;
    b[0] = (unsigned char)(v >> 24);
    b[1] = (unsigned char)(v >> 16);
    b[2] = (unsigned char)(v >> 8);
    b[3] = (unsigned char)v;
}
static __inline__ unsigned short pc_be16_get(const void *p) {
    const unsigned char *b = (const unsigned char *)p;
    return (unsigned short)((b[0] << 8) | b[1]);
}
static __inline__ void pc_be16_set(void *p, unsigned short v) {
    unsigned char *b = (unsigned char *)p;
    b[0] = (unsigned char)(v >> 8);
    b[1] = (unsigned char)v;
}

/* A colour word as the N64 holds it in a register, 0xRRGGBBAA: the word whose bytes in host memory are R, G, B, A.
 * That is how the game's byte readers (the model draw's prim and env, the fill and font colours) and the renderer
 * take a colour. For a word the game stores into colour storage that is read as bytes: a constant, or a colour read
 * from data the PC loader swapped as a u32 (docs/port/ENDIAN.md). */
#define PC_RGBA(x) __builtin_bswap32(x)

/* N64 addresses (K0, K1 or physical) the game computes as integers, and back (port/ultra). */
void *pc_n64_ptr(unsigned int n64_addr);
unsigned int pc_n64_addr(const void *host);

/* game.memory's window past 16 MiB (docs/port/MEMORY.md 3), where a physical address would read as a segmented one:
 * the display-list relocations (relocate_model_dl, relocate_texture_loads) give the K0 address there (pc_rcp_reloc: the
 * N64's `((w1 + base) & 0xFFFFFF) | segment` below), and display_list_traverse and the MusyX library's RAM test take
 * such a K0 address as the RAM address it is (pc_rdram_k0_high). At game.memory = original each is the N64's rule. */
unsigned int pc_rcp_reloc(unsigned int w1, unsigned int base, int seg);
int pc_rdram_k0_high(unsigned int addr);

/* The game heap (port/ultra/heap.c, docs/port/MEMORY.md 4): the allocator says when the heap is made and after each
 * block it takes, under the heap's lock; an allocation no free block can hold, and the game's own fallbacks when it
 * finds the heap short (LoadCars, a car's paint), are logged the first time and counted. */
void pc_heap_created(void *heap);
void pc_heap_allocated(void *heap);
void pc_heap_none(void *heap, unsigned int size, unsigned int free_bytes, unsigned int largest);
void pc_heap_short(const char *what, unsigned int need);

/* The one translation between a 32-bit N64 address and a host pointer (docs/port/NOFIXED.md 2.3), for game C that
 * crosses that boundary: PC_N64P(a) is the host pointer for N64 address a, for a cast to the pointer's type
 * ((T *)PC_N64P(a): the N64P definitions, an N64 address the game computes), and PC_N64A(p) the 32-bit N64 address of
 * a host pointer into the RDRAM window (the heap's arithmetic, a block's address relocated into a file). In the
 * fixed build (the default) the window is at host 0x80000000 and they are the zero-extension and the truncation they
 * stand for, so that build compiles as it did. A RUSH_NOFIXED build's window is anywhere, at pc_window_base
 * (port/ultra/n64mem.c, which pc_mem_init sets): K0, K1 and physical addresses are pc_window_base + (a & 0x1FFFFFFF),
 * a pointer p is 0x80000000 + (p - pc_window_base), and 0 and NULL are each other. */
#ifdef RUSH_NOFIXED
extern unsigned char *pc_window_base;
static __inline__ void *pc_n64p(unsigned int a)
{
    return a ? (void *)(pc_window_base + (a & 0x1FFFFFFFu)) : (void *)0;
}
static __inline__ unsigned int pc_n64a(__UINTPTR_TYPE__ p)
{
    return p ? 0x80000000u + (unsigned int)(p - (__UINTPTR_TYPE__)pc_window_base) : 0u;
}
#define PC_N64P(x) pc_n64p((unsigned int)(x))
#define PC_N64A(x) pc_n64a((__UINTPTR_TYPE__)(x))
#else
#define PC_N64P(x) ((__UINTPTR_TYPE__)(unsigned int)(x))
#define PC_N64A(x) ((unsigned int)(__UINTPTR_TYPE__)(x))
#endif

/* The 32-bit N64 address a pointer stands for where the game keeps one as a number, an id (port/ultra/statehash.c):
 * a window pointer's own, own storage's its symbol's N64 address and offset as on the N64, else the low half (as
 * PC_N64A). A profile's id is its handle's (save/group.c), and the handles are own storage: cut to 32
 * bits it was the process's own (ASLR), in the pak's saves and the game's memory (docs/port/LOCKSTEP.md 5). */
unsigned int pc_n64_identity(const void *p);

/* A heap handle's slot (AllocateHandleHeap, memsys/group.c) is 8 bytes on the PC and holds its block's host
 * pointer, which the game reads through the handle (`*(u8 **)h`, docs/port/POINTERS.md 9.10); the heap and a few
 * readers take the slot as the N64's u32 instead. PC_SLOT_N64(s) is the N64 address the slot at host address s holds,
 * PC_SLOT_SET(s, a) stores N64 address a's pointer in it. With the window at 0x80000000 the pointer is that address
 * zero-extended, so the N64's 4-byte read and write of the slot's first half are what they do; a RUSH_NOFIXED build's
 * pointer is taken whole. */
#ifdef RUSH_NOFIXED
#define PC_SLOT_N64(s) PC_N64A(*(void **)(s))
#define PC_SLOT_SET(s, a) (*(void **)(s) = PC_N64P(a))
#else
#define PC_SLOT_N64(s) (*(unsigned int *)(s))
#define PC_SLOT_SET(s, a) (*(unsigned int *)(s) = (a))
#endif

/* A pointer stored as a 32-bit N64 address: the u32 fields of records that keep their N64 layout (each file's
 * N64W(x) is PC_N64W(x)). That is right for RDRAM-window addresses only. A RUSH_CHECK_CASTS build
 * (cmake -DRUSH_CHECK_CASTS=ON) checks every such store and stops at the first one that would drop the upper half
 * of a host address, naming the source line (pc_cast_fail, port/ultra/castcheck.c; docs/port/CASTS.md). A RUSH_NOFIXED
 * build translates (PC_N64A) and always checks: a pointer outside the window (PC_RDRAM_MAX bytes from its start)
 * stops there too. */
#if defined(RUSH_NOFIXED)
void pc_cast_fail(const char *file, int line, __UINTPTR_TYPE__ value) __attribute__((weak));
static __inline__ unsigned int pc_n64w_moved(__UINTPTR_TYPE__ v, const char *file, int line)
{
    if (v && v - (__UINTPTR_TYPE__)pc_window_base >= 0x10000000u)
        pc_cast_fail(file, line, v);   /* weak: a program without port/ultra faults here */
    return pc_n64a(v);
}
#define PC_N64W(x) pc_n64w_moved((__UINTPTR_TYPE__)(x), __FILE__, __LINE__)
#elif defined(RUSH_CHECK_CASTS)
void pc_cast_fail(const char *file, int line, __UINTPTR_TYPE__ value) __attribute__((weak));
static __inline__ unsigned int pc_n64w_checked(__UINTPTR_TYPE__ v, const char *file, int line)
{
    if (v >> 16 >> 16)
        pc_cast_fail(file, line, v);   /* weak: a program without port/ultra faults here */
    return (unsigned int)v;
}
#define PC_N64W(x) pc_n64w_checked((__UINTPTR_TYPE__)(x), __FILE__, __LINE__)
#else
#define PC_N64W(x) ((unsigned int)(__UINTPTR_TYPE__)(x))
#endif

/* pc_os_spin_yield(site) goes in the body of a busy-wait: the writer it waits for (another N64 thread, or what the
 * N64 did from an interrupt) gets to run (port/ultra/sched.c). pc_os_idle() is the idle thread's loop. */
void pc_os_spin_yield(const char *site);
void pc_os_idle(void);

/* A car visual (tVisual: N64 0x18 bytes, PC 0x20), struct pc_visual, is declared with the car record in
 * pc_game_types.h (included at the end), for views of the car record that pad over the car's 21 visuals. */

/* NewMultiBlit's descriptor (NewMultiBlit; MultiBlit, arcade LIB/blit.h): N64 36 bytes, PC 48, its texture name and
 * animation function being pointers. A file whose declaration of such a table has no layout (s32, u8[]) declares it
 * with this one on the PC, so the data loader gives the table its PC layout (docs/port/BRINGUP.md 1). */
struct pc_multiblit {
    const char *texname;
    short dulx, duly, width, height, top, bot, left, right;
    unsigned int zdepth, alpha;
    int (*animfunc)(void *);
    unsigned int animid;
};

/* The car record (gGameCar, 6 cars) is 0x468 bytes on the PC, 0x3B8 on the N64: its visuals are 0x20 bytes
 * from +0x110 to N64 +0x308, and its input record pointer at N64 +0x380 is 8 bytes. CAR_OFS(n) is the PC offset of
 * N64 offset n, which may run into the next cars (CAR_OFS(0x3B8) is the PC stride). Code that reaches cars by raw
 * offsets goes through it. */
#define PC_CAR_SIZE 0x468
#define PC_CAR_REL(r) ((r) < 0x110 ? (r) : (r) < 0x308 ? 0x110 + ((r) - 0x110) / 0x18 * 0x20 \
    + (((r) - 0x110) % 0x18 < 4 ? ((r) - 0x110) % 0x18 : ((r) - 0x110) % 0x18 + 4) : (r) <= 0x380 ? (r) + 0xA8 \
    : (r) + 0xAC)
#define CAR_OFS(n) ((n) / 0x3B8 * PC_CAR_SIZE + PC_CAR_REL((n) % 0x3B8))
/* The PC offset of N64 offset x inside a car visual, up to its stride (VIS_OFS(0x18) is 0x20). */
#define VIS_OFS(x) (CAR_OFS(0x110 + (x)) - 0x110)

/* The save records (docs/port/POINTERS.md 9.10). Each is RAM-only and starts with pointers, so on the PC the rest
 * of it is later; X_OFS(n) is the PC offset of N64 offset n, for code that reaches the record by raw offsets (the
 * N64 size gives the PC size, as CAR_OFS does).
 * - The pak file record (paksys' tPS_File, pak_file_records[64]; N64 76 bytes, PC 96): next, prev, RemoveFile and
 *   MoveFile at +0/4/8/12 are pointers, then pak, fileNo, name[35], ext[11], size, blocks and the data handle. */
#define PC_PSFILE_SIZE 96
#define PC_PSFILE_REL(r) ((r) < 4 ? (r) : (r) < 8 ? (r) + 4 : (r) < 12 ? (r) + 8 : (r) < 16 ? (r) + 12 : (r) + 16)
#define PSF_OFS(n) ((n) / 76 * PC_PSFILE_SIZE + PC_PSFILE_REL((n) % 76))
/* - The profile record (save.c's SaveSlot, GC tPlayer; N64 48 bytes, PC 64): next, prev and the pak file handle at
 *   +0/4/8, the id, name and controls from +12, and the save data handle at +0x2C. */
#define PC_PROFILE_SIZE 64
#define PC_PROFILE_REL(r) ((r) < 4 ? (r) : (r) < 8 ? (r) + 4 : (r) < 12 ? (r) + 8 : (r) + 12)
#define PROFILE_OFS(n) ((n) / 48 * PC_PROFILE_SIZE + PC_PROFILE_REL((n) % 48))
/* - A ghost (SaveInit' Ghost; N64 0x2C bytes, PC 64): the link and the file handle at +0/4, the data and
 *   header handles at +0x24/0x28. */
#define PC_GHOST_SIZE 64
#define PC_GHOST_REL(r) ((r) < 4 ? (r) : (r) < 8 ? (r) + 4 : (r) < 0x24 ? (r) + 8 : (r) < 0x28 ? (r) + 12 : (r) + 16)
#define GHOST_OFS(n) ((n) / 0x2C * PC_GHOST_SIZE + PC_GHOST_REL((n) % 0x2C))

/* The target info table (targetInfo, MAT3 targetInfo, 122 entries) is 0x40 bytes an entry on the PC, 0x30 on the
 * N64: its first four words are pointers (the name, the model file, the start function and the visual's function),
 * so everything after them is 0x10 further on. TINFO_OFS(n) is the PC offset of N64 offset n (TINFO_OFS(0x30) is
 * the stride; docs/port/POINTERS.md 10). */
#define TINFO_OFS(n) ((n) / 0x30 * 0x40 + ((n) % 0x30 < 0x10 ? (n) % 0x30 * 2 : (n) % 0x30 + 0x10))

/* A StatLL list links its elements through their first word, and a doubly linked list back through the second.
 * Link k of element e: a host pointer, or a 32-bit window address when the list's header is flagged n64 (its
 * elements keep their N64 layout: the level-object nodes, docs/port/POINTERS.md 10). */
static __inline__ void *pc_statll_link(int n64, void *e, int k) {
    return n64 ? (void *)PC_N64P(((unsigned int *)e)[k]) : ((void **)e)[k];
}
static __inline__ void pc_statll_set_link(int n64, void *e, int k, void *v) {
    if (n64)
        ((unsigned int *)e)[k] = PC_N64W(v);
    else
        ((void **)e)[k] = v;
}

/* Ranges that hold raw big-endian ROM bytes (port/ultra, pc_port.h). */
void pc_mem_register_be(const void *start, __SIZE_TYPE__ size);
void pc_mem_unregister_be(const void *start);

/* A wave's file is in memory, or its block goes away (port/assets, pc_port.h). */
void pc_wave_loaded(int wave, void *buf, __SIZE_TYPE__ size);
void pc_wave_released(int wave, void *buf);

/* IDO's float (or double) to unsigned int, `(u32)f` in the game C: cvt.w.s rounding toward zero; at 2^31 or more
 * again on f - 2^31 with the top bit set; a negative result (f <= -1), NaN or 2^32 and more give 0xFFFFFFFF, and
 * (-1, 0) gives 0. C leaves these undefined, and the PC compilers differ: x86-64 wraps a negative value (-3 gives
 * 0xFFFFFFFD), AArch64 saturates it to 0 (docs/port/AUDIO.md 9). */
static __inline__ unsigned int pc_mips_f2u(float f)
{
    if (!(f > -1.0f))
        return 0xFFFFFFFFu;
    if (f < 2147483648.0f)
        return (unsigned int)(int)f;
    if (f < 4294967296.0f)
        return (unsigned int)(int)(f - 2147483648.0f) | 0x80000000u;
    return 0xFFFFFFFFu;
}
static __inline__ unsigned int pc_mips_d2u(double d)
{
    if (!(d > -1.0))
        return 0xFFFFFFFFu;
    if (d < 2147483648.0)
        return (unsigned int)(int)d;
    if (d < 4294967296.0)
        return (unsigned int)(int)(d - 2147483648.0) | 0x80000000u;
    return 0xFFFFFFFFu;
}
#define N64_F2U(x) pc_mips_f2u(x)
#define N64_D2U(x) pc_mips_d2u(x)

/* Game data the PC owns, with no N64 address: the race roster, the model records, the car records
 * (docs/port/DECOUPLE.md). */
#include "pc_game_types.h"

/* game.frame_rate: the game's own frame rate and the per-frame steps that follow it (docs/port/FRAMERATE.md 7). */
#include "pc_game_fps.h"

#endif
