/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned int u32;
typedef int s32;
s32 seeded_rand(u32 *seed) {
    *seed = *seed * 1103515245 + 12345;
    return (*seed >> 16) & 0x7fff;
}
