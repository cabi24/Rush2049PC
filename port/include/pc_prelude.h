/* port/include/pc_prelude.h: force-included first into every game C file on TARGET_PC (CMakeLists.txt,
 * game_flags). The N64 build never sees it. No system headers: each game file carries its own declarations.
 * docs/port/POINTERS.md describes how the game C uses it. */
#ifndef PC_PRELUDE_H
#define PC_PRELUDE_H

/* Pointer-sized integers. Game files that use them declare the N64 versions (u32/s32) themselves, under
 * #ifndef TARGET_PC. */
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
 * from data the PC loader swapped as a u32 (docs/port/ENDIAN.md). Each game file that uses it defines the N64 side:
 *     #ifndef TARGET_PC
 *     #define PC_RGBA(x) x
 *     #endif */
#define PC_RGBA(x) __builtin_bswap32(x)

/* N64 addresses (K0, K1 or physical) the game computes as integers, and back (port/ultra). */
void *pc_n64_ptr(unsigned int n64_addr);
unsigned int pc_n64_addr(const void *host);

/* A pointer stored as a 32-bit N64 address: the u32 fields of records that keep their N64 layout (each file's
 * N64W(x) is PC_N64W(x)). That is right for RDRAM-window addresses only. A RUSH_CHECK_CASTS build
 * (cmake -DRUSH_CHECK_CASTS=ON) checks every such store and stops at the first one that would drop the upper half
 * of a host address, naming the source line (pc_cast_fail, port/ultra/castcheck.c; docs/port/CASTS.md). */
#ifdef RUSH_CHECK_CASTS
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

/* Statements only one build has, placed on a line the function already has, so its lines stay as IDO
 * compiles them (tools/port/n64_neutral.py). Each game file that uses them defines the N64 side itself:
 *     #ifndef TARGET_PC
 *     #define PC_SPIN(site)
 *     #endif
 * PC_SPIN goes in the body of a busy-wait: the writer it waits for (another N64 thread, or what the N64 did
 * from an interrupt) gets to run (port/ultra/sched.c). PC_IDLE is the idle thread's loop. N64_ONLY(stmt)
 * keeps a statement from the PC build, PC_ONLY(stmt) from the N64 one. */
void pc_os_spin_yield(const char *site);
void pc_os_idle(void);
#define PC_SPIN(site) pc_os_spin_yield(site);
#define PC_IDLE() pc_os_idle();
#define N64_ONLY(stmt)
#define PC_ONLY(stmt) stmt

/* A car visual (tVisual: N64 0x18 bytes, PC 0x20) on the PC, for views of the car record that pad over the
 * car's 21 visuals at +0x110: next and func are host pointers there, data a u32 (docs/port/CARS.md). */
struct pc_visual {
    void *next;
    short index, objnum, slot;
    unsigned int data;
    float timeStamp;
    void (*func)(void *, short);
};

/* NewMultiBlit's descriptor (sound_control; MultiBlit, arcade LIB/blit.h): N64 36 bytes, PC 48, its texture name and
 * animation function being pointers. A file whose declaration of such a table has no layout (s32, u8[]) declares it
 * with this one on the PC, so the data loader gives the table its PC layout (docs/port/BRINGUP.md 1). */
struct pc_multiblit {
    const char *texname;
    short dulx, duly, width, height, top, bot, left, right;
    unsigned int zdepth, alpha;
    int (*animfunc)(void *);
    unsigned int animid;
};

/* The car record (player_array, 6 cars) is 0x468 bytes on the PC, 0x3B8 on the N64: its visuals are 0x20 bytes
 * from +0x110 to N64 +0x308, and its input record pointer at N64 +0x380 is 8 bytes. CAR_OFS(n) is the PC offset of
 * N64 offset n, which may run into the next cars (CAR_OFS(0x3B8) is the PC stride). Code that reaches cars by raw
 * offsets goes through it; each game file that does defines the N64 side, `#define CAR_OFS(n) n`. */
#define PC_CAR_SIZE 0x468
#define PC_CAR_REL(r) ((r) < 0x110 ? (r) : (r) < 0x308 ? 0x110 + ((r) - 0x110) / 0x18 * 0x20 \
    + (((r) - 0x110) % 0x18 < 4 ? ((r) - 0x110) % 0x18 : ((r) - 0x110) % 0x18 + 4) : (r) <= 0x380 ? (r) + 0xA8 \
    : (r) + 0xAC)
#define CAR_OFS(n) ((n) / 0x3B8 * PC_CAR_SIZE + PC_CAR_REL((n) % 0x3B8))
/* The PC offset of N64 offset x inside a car visual, up to its stride (VIS_OFS(0x18) is 0x20). */
#define VIS_OFS(x) (CAR_OFS(0x110 + (x)) - 0x110)

/* The save records (docs/port/POINTERS.md 9.10). Each is RAM-only and starts with pointers, so on the PC the rest
 * of it is later; X_OFS(n) is the PC offset of N64 offset n, for code that reaches the record by raw offsets (the
 * N64 size gives the PC size, as CAR_OFS does). Game files that use one define the N64 side, `#define PSF_OFS(n) n`.
 * - The pak file record (paksys' tPS_File, D_80144DC0[64]; N64 76 bytes, PC 96): next, prev, RemoveFile and
 *   MoveFile at +0/4/8/12 are pointers, then pak, fileNo, name[35], ext[11], size, blocks and the data handle. */
#define PC_PSFILE_SIZE 96
#define PC_PSFILE_REL(r) ((r) < 4 ? (r) : (r) < 8 ? (r) + 4 : (r) < 12 ? (r) + 8 : (r) < 16 ? (r) + 12 : (r) + 16)
#define PSF_OFS(n) ((n) / 76 * PC_PSFILE_SIZE + PC_PSFILE_REL((n) % 76))
/* - The profile record (save.c's SaveSlot, GC tPlayer; N64 48 bytes, PC 64): next, prev and the pak file handle at
 *   +0/4/8, the id, name and controls from +12, and the save data handle at +0x2C. */
#define PC_PROFILE_SIZE 64
#define PC_PROFILE_REL(r) ((r) < 4 ? (r) : (r) < 8 ? (r) + 4 : (r) < 12 ? (r) + 8 : (r) + 12)
#define PROFILE_OFS(n) ((n) / 48 * PC_PROFILE_SIZE + PC_PROFILE_REL((n) % 48))
/* - A ghost (menu_save_options' Ghost; N64 0x2C bytes, PC 64): the link and the file handle at +0/4, the data and
 *   header handles at +0x24/0x28. */
#define PC_GHOST_SIZE 64
#define PC_GHOST_REL(r) ((r) < 4 ? (r) : (r) < 8 ? (r) + 4 : (r) < 0x24 ? (r) + 8 : (r) < 0x28 ? (r) + 12 : (r) + 16)
#define GHOST_OFS(n) ((n) / 0x2C * PC_GHOST_SIZE + PC_GHOST_REL((n) % 0x2C))

/* The target info table (D_80117530, MAT3 targetInfo, 122 entries) is 0x40 bytes an entry on the PC, 0x30 on the
 * N64: its first four words are pointers (the name, the model file, the start function and the visual's function),
 * so everything after them is 0x10 further on. TINFO_OFS(n) is the PC offset of N64 offset n (TINFO_OFS(0x30) is
 * the stride); a game file that uses it defines the N64 side, `#define TINFO_OFS(n) n` (docs/port/POINTERS.md 10). */
#define TINFO_OFS(n) ((n) / 0x30 * 0x40 + ((n) % 0x30 < 0x10 ? (n) % 0x30 * 2 : (n) % 0x30 + 0x10))

/* A StatLL list links its elements through their first word, and a doubly linked list back through the second.
 * Link k of element e: a host pointer, or a 32-bit window address when the list's header is flagged n64 (its
 * elements keep their N64 layout: the level-object nodes, docs/port/POINTERS.md 10). */
static __inline__ void *pc_statll_link(int n64, void *e, int k) {
    return n64 ? (void *)(__UINTPTR_TYPE__)((unsigned int *)e)[k] : ((void **)e)[k];
}
static __inline__ void pc_statll_set_link(int n64, void *e, int k, void *v) {
    if (n64)
        ((unsigned int *)e)[k] = (unsigned int)(__UINTPTR_TYPE__)v;
    else
        ((void **)e)[k] = v;
}

/* Ranges that hold raw big-endian ROM bytes (port/ultra, pc_port.h). */
void pc_mem_register_be(const void *start, __SIZE_TYPE__ size);
void pc_mem_unregister_be(const void *start);

/* A wave's file is in memory, or its block goes away (port/assets, pc_port.h). */
void pc_wave_loaded(int wave, void *buf, __SIZE_TYPE__ size);
void pc_wave_released(int wave, void *buf);

#endif
