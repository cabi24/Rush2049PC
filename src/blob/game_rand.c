/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned int u32;
typedef int s32;
typedef signed char s8;

/* libc ANSI rand(): seed at rand_seed. No callers (all inlined elsewhere). */
extern int rand_seed;
int game_rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7fff;
}
