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

/* N64 addresses (K0, K1 or physical) the game computes as integers, and back (port/ultra). */
void *pc_n64_ptr(unsigned int n64_addr);
unsigned int pc_n64_addr(const void *host);

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

/* Ranges that hold raw big-endian ROM bytes (port/ultra, pc_port.h). */
void pc_mem_register_be(const void *start, __SIZE_TYPE__ size);
void pc_mem_unregister_be(const void *start);

/* A wave's file is in memory, or its block goes away (port/assets, pc_port.h). */
void pc_wave_loaded(int wave, void *buf, __SIZE_TYPE__ size);
void pc_wave_released(int wave, void *buf);

#endif
