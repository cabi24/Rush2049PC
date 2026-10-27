/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
#ifdef TARGET_PC
/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 s22; s16 s24; s16 mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Ent;
#else
typedef struct { u8 pad0[22]; s16 s22; s16 s24; u8 pad26[42]; } Ent;
#endif
extern Ent D_8012E700[];
extern s16 D_8015B254;
s32 func_8008E144(s32);
void render_mode_select(s16 a, s16 b) {
    if (b < 0) {
        if (D_8015B254 < 0) {
            D_8015B254 = a;
        } else {
            D_8012E700[func_8008E144(D_8015B254)].s24 = a;
        }
    } else {
        if (D_8012E700[b].s22 == -1) {
            D_8012E700[b].s22 = a;
        } else {
            D_8012E700[func_8008E144(D_8012E700[b].s22)].s24 = a;
        }
    }
}
