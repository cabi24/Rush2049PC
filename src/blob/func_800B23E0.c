/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Pick a random bit number 0..31 allowed by the mask D_80123418[index]
 * (rejection sampling; a zero mask never terminates). N64 code; the
 * random source is the arcade LIB/fmath.c Random(F32 max):
 *     rannum = ((F32)(rand() & 0x07FFF) * max) / 32767.0;   (N64: 32768.0f)
 * over libc rand() (seed D_8011735C, kept func_8008B2B4), i.e. the kept
 * func_8008B2E4 = Random, both inlined here by umerge in the unit.
 * the whole-program unit score is EQUAL with the plain call `bit = func_8008B2E4(32.0f);`
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * Standalone, static copies of rand/Random reproduce it.
 *
 * Quirks: the arcade's redundant `& 0x07FFF` on rand()'s already-masked
 * result costs a temp (t0/t1 instead of t9/v0) and the named `rannum`
 * local; the mask is read in the loop condition (a named mask local is
 * 14 words off); the result is a u8.
 */
typedef float f32;
typedef unsigned char u8;
typedef unsigned int u32;

extern int D_8011735C;
extern u32 D_80123418[];

int func_8008B2B4(void);

f32 func_8008B2E4(f32 max);

u8 func_800B23E0(u8 index)
{
    u8 bit;

    do {
        bit = func_8008B2E4(32.0f);
    } while (!(D_80123418[index] & (1 << bit)));
    return bit;
}
