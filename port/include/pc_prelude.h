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

/* A wave's file is in memory, or its block goes away (port/assets, pc_port.h). */
void pc_wave_loaded(int wave, void *buf, __SIZE_TYPE__ size);
void pc_wave_released(int wave, void *buf);

#endif
