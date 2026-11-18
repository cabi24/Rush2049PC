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

/* The cartridge (port/ultra/pi.c): size bytes at a ROM offset read into dst at once, then an empty message to done, the
 * OSMesgQueue its reader waits on (NULL: none). */
void pc_cart_read(unsigned int rom_offset, void *dst, unsigned int size, void *done);

/* The video interface (port/ultra/vi.c): the screen blacked out (1) or not (0); a frame buffer shown from the next
 * retrace on; the frame buffers shown now and from the next retrace (the same when no swap waits). */
void pc_vi_black(int black);
void pc_vi_swap_buffer(void *framebuffer);
void *pc_vi_current_framebuffer(void);
void *pc_vi_next_framebuffer(void);

/* A visual (tVisual: N64 0x18 bytes, PC 0x28), struct pc_visual, is declared with the car record in
 * pc_game_types.h (included at the end), for views of the car record that pad over the car's 21 visuals. */

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


/* The target info table (targetInfo, MAT3 targetInfo, 122 entries) is 0x40 bytes an entry on the PC, 0x30 on the
 * N64: its first four words are pointers (the name, the model file, the start function and the visual's function),
 * so everything after them is 0x10 further on. TINFO_OFS(n) is the PC offset of N64 offset n (TINFO_OFS(0x30) is
 * the stride; docs/port/POINTERS.md 10). */
#define TINFO_OFS(n) ((n) / 0x30 * 0x40 + ((n) % 0x30 < 0x10 ? (n) % 0x30 * 2 : (n) % 0x30 + 0x10))


/* Sample memory: the sound library's caches of the cartridge's samples and codebooks, which the audio interpreter reads
 * as the cartridge holds them (port/ultra, pc_port.h). */
void pc_mem_register_samples(const void *start, __SIZE_TYPE__ size);
void pc_mem_unregister_samples(const void *start);

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
