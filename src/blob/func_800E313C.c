/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef signed char s8; typedef signed short s16; typedef signed int s32; typedef unsigned char u8; typedef float f32;
#define M2C_FIELD(e,t,o) (*(t)((u8 *)(e)+(o)))
extern f32 D_80110EBC[][6];
#ifdef TARGET_PC
/* PC: the model record keeps its N64 layout; its descriptor (+0) and tune (+4) pointers are u32 window
 * addresses (docs/port/CARS.md). */
#define M2C_N64P(e, o) ((void *) (uintptr_t) M2C_FIELD(e, unsigned int *, o))
#else
#define M2C_N64P(e, o) M2C_FIELD(e, void **, o)
#endif
void func_800E313C(void *arg0, f32 arg1, f32 arg2) {
    M2C_FIELD(arg0, s16 *, 0x3F4) = 1;
    if (M2C_FIELD(arg0, s16 *, 0x3F4) < 4) {
loop_1:
        {
        f32 product = (M2C_FIELD(M2C_N64P(arg0, 4), f32 *, 0xC) * M2C_FIELD(M2C_N64P(arg0, 0), f32 *, 0xA4)) * D_80110EBC[M2C_FIELD(arg0,s8 *,9)][M2C_FIELD(arg0,s16 *,0x3F4)+1];
        f32 value = M2C_FIELD(arg0, f32 *, 0x41C) * product;
        if (!(value < arg1)) {
            M2C_FIELD(arg0, s16 *, 0x3F4) = (s16) (M2C_FIELD(arg0, s16 *, 0x3F4) + 1);
            if (M2C_FIELD(arg0, s16 *, 0x3F4) < 4) {
                goto loop_1;
            }
        }
        }
    }
}
