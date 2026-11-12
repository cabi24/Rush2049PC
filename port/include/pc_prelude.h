/* port/include/pc_prelude.h: force-included first into every game C file (CMakeLists.txt, game_flags). No system
 * headers: each game file carries its own declarations. docs/port/POINTERS.md describes how the game C uses it. */
#ifndef PC_PRELUDE_H
#define PC_PRELUDE_H

/* What is left of the N64's memory map: the cartridge's bus addresses and the display lists' segments. */
#include "pc_n64_map.h"

/* Pointer-sized integers. */
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTPTR_TYPE__ intptr_t;

/* A palette's colour (a TLUT entry, RGBA 5551) where a file holds it: the files' texels and TLUTs keep the N64's texel
 * formats, which the renderer decodes, and a 16-bit texel there is high byte first (docs/port/NATIVE.md 4.4). The
 * game reads and writes palette colours one at a time with these (the car paint's copy, the texture animation's
 * lights); moving them as they are needs nothing. */
static __inline__ unsigned short pc_tlut_get(const void *entry) {
    const unsigned char *b = (const unsigned char *)entry;
    return (unsigned short)((b[0] << 8) | b[1]);
}
static __inline__ void pc_tlut_set(void *entry, unsigned short colour) {
    unsigned char *b = (unsigned char *)entry;
    b[0] = (unsigned char)(colour >> 8);
    b[1] = (unsigned char)colour;
}

/* A colour word as the N64 holds it in a register, 0xRRGGBBAA: the word whose bytes in host memory are R, G, B, A.
 * That is how the game's byte readers (the model draw's prim and env, the fill and font colours) and the renderer
 * take a colour. For a word the game stores into colour storage that is read as bytes: a constant, or a colour read
 * from data the PC loader swapped as a u32 (docs/port/ENDIAN.md). */
#define PC_RGBA(x) __builtin_bswap32(x)

/* The game heap (port/ultra/heap.c, docs/port/MEMORY.md 4): pc_heap_memory is the memory the main heap manages on a
 * console with mem_size bytes of RDRAM (osMemSize), its size in *size (locate_main_heap). The allocator says when the
 * heap is made and after each block it takes, under the heap's lock; an allocation no free block can hold, and the
 * game's own fallbacks when it finds the heap short (LoadCars, a car's paint), are logged the first time and
 * counted. */
void *pc_heap_memory(unsigned int mem_size, unsigned int *size);
void pc_heap_created(void *heap);
void pc_heap_allocated(void *heap);
void pc_heap_none(void *heap, unsigned int size, unsigned int free_bytes, unsigned int largest);
void pc_heap_short(const char *what, unsigned int need);

/* The 32-bit id the N64 game made of a pointer where it keeps one as a number (port/ultra/statehash.c): a profile's id
 * is its handle's address (save/group.c), which the pak's saves hold. The handles are own storage, which the data
 * loader's tables give their N64 address (and an arena's datum its own), so the id, and the saves, are the N64's, the
 * same in every process and on every OS (docs/port/LOCKSTEP.md 5); any other pointer gives its low half. */
unsigned int pc_n64_identity(const void *p);

/* pc_os_spin_yield(site) goes in the body of a busy-wait: the writer it waits for (another N64 thread, or what the
 * N64 did from an interrupt) gets to run (port/ultra/sched.c). pc_os_idle() is the idle thread's loop. */
void pc_os_spin_yield(const char *site);
void pc_os_idle(void);

/* A visual (tVisual: N64 0x18 bytes, PC 0x28), struct pc_visual, is declared with the car record in
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

/* The car record (gGameCar, 6 cars) is 0x510 bytes on the PC, 0x3B8 on the N64: its 21 visuals are 0x28 bytes (struct
 * pc_visual: next, the data word and func pointer-sized) from +0x110 to N64 +0x308, and its input record pointer at
 * N64 +0x380 is 8 bytes. CAR_OFS(n) is the PC offset of N64 offset n, which may run into the next cars (CAR_OFS(0x3B8)
 * is the PC stride). Code that reaches cars by raw offsets goes through it. Inside a visual the N64's next (+0) stays
 * at +0, index, objnum and slot (+4..+0xA) and data (+0xC) move 4 bytes on, timeStamp (+0x10) 8 and func (+0x14) 0xC. */
#define PC_CAR_SIZE 0x510
#define PC_VIS_REL(o) ((o) < 4 ? (o) : (o) < 0x10 ? (o) + 4 : (o) < 0x14 ? (o) + 8 : (o) + 0xC)
#define PC_CAR_REL(r) ((r) < 0x110 ? (r) : (r) < 0x308 ? 0x110 + ((r) - 0x110) / 0x18 * 0x28 \
    + PC_VIS_REL(((r) - 0x110) % 0x18) : (r) <= 0x380 ? (r) + 0x150 : (r) + 0x154)
#define CAR_OFS(n) ((n) / 0x3B8 * PC_CAR_SIZE + PC_CAR_REL((n) % 0x3B8))
/* The PC offset of N64 offset x inside a car visual, up to its stride (VIS_OFS(0x18) is 0x28). */
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
