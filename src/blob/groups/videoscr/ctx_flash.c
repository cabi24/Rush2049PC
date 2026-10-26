/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Context: func_800BEA3C = TB_FlashTextColors, the function text byte-identical to its definition
 * src/blob/groups/audioscr/audio.c (member there, kept). Present so the standalone group compile can
 * inline it into controls_animButtonText as the unit does (retail: the by-value parameter homes at sp+88/92 and
 * sp+104/108 of controls_animButtonText). */
typedef unsigned char u8;
typedef unsigned int u32;
typedef union TextColor { u8 c[4]; u32 rgba; } TextColor;   /* GC union __anon_21da */
extern TextColor D_80118E28, D_80118E2C;
void func_800BEA3C(TextColor first, TextColor second)   /* TB_FlashTextColors (GC: two union __anon_21da) */
{
    D_80118E28.rgba = first.rgba;
    D_80118E2C.rgba = second.rgba;
}
