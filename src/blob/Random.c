/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Random(max): arcade LIB/fmath.c
 *     F32 Random(F32 max) { F32 rannum;
 *         rannum = ( ((F32)(rand() & 0x07FFF)*max) / 32767.0); return(rannum); }
 * with the N64 divisor 32768.0f (0x47000000). rand() is the kept libc
 * rand game_rand (seed rand_seed), inlined by umerge in the unit:
 * the whole-program unit score is EQUAL with `game_rand & 0x07FFF`
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * static copy reproduces it.
 * Quirk: the redundant `& 0x07FFF` (rand already masks) is what shifts the
 * temp ring to t0/t1 and leaves the seed address in v1.
 * No jal callers: inlined into its callers (e.g. pick_random_masked_bit).
 */
typedef float f32;

extern int rand_seed;

int game_rand(void);

f32 Random(f32 max)
{
    f32 rannum;

    rannum = ((f32)(game_rand() & 0x07FFF) * max) / 32768.0f;
    return rannum;
}
