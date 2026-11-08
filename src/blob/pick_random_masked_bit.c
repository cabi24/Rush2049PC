/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Pick a random bit number 0..31 allowed by the mask random_bit_masks[index]
 * (rejection sampling; a zero mask never terminates). N64 code; the
 * random source is the arcade LIB/fmath.c Random(F32 max):
 *     rannum = ((F32)(rand() & 0x07FFF) * max) / 32767.0;   (N64: 32768.0f)
 * over libc rand() (seed rand_seed, kept game_rand), i.e. the kept
 * Random = Random, both inlined here by umerge in the unit.
 * the whole-program unit score is EQUAL with the plain call `bit = Random(32.0f);`
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

extern int rand_seed;
extern u32 random_bit_masks[];

int game_rand(void);

f32 Random(f32 max);

u8 pick_random_masked_bit(u8 index)
{
    u8 bit;

    do {
        bit = Random(32.0f);
    } while (!(random_bit_masks[index] & (1 << bit)));
    return bit;
}
