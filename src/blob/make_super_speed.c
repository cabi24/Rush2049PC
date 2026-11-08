/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef signed char s8; typedef signed short s16; typedef signed int s32; typedef unsigned char u8; typedef float f32;
#define M2C_FIELD(e,t,o) (*(t)((u8 *)(e)+(o)))
extern f32 invmagnitude(f32 *); extern f32 super_speed_scale_x,super_speed_scale_y,super_speed_scale_z;
void make_super_speed(void *arg0) {
    f32 temp_f2;
    f32 temp_f2_2;
    f32 temp_f2_3;

    temp_f2 = invmagnitude((f32 *)((u8 *)arg0+0x2EC)) * super_speed_scale_x;
    M2C_FIELD(arg0, f32 *, 0x2EC) = (f32) (M2C_FIELD(arg0, f32 *, 0x2EC) * temp_f2);
    M2C_FIELD(arg0, f32 *, 0x2F0) = (f32) (M2C_FIELD(arg0, f32 *, 0x2F0) * temp_f2);
    M2C_FIELD(arg0, f32 *, 0x2F4) = (f32) (M2C_FIELD(arg0, f32 *, 0x2F4) * temp_f2);
    temp_f2_2 = invmagnitude((f32 *)((u8 *)arg0+0x2F8)) * super_speed_scale_y;
    M2C_FIELD(arg0, f32 *, 0x2F8) = (f32) (M2C_FIELD(arg0, f32 *, 0x2F8) * temp_f2_2);
    M2C_FIELD(arg0, f32 *, 0x2FC) = (f32) (M2C_FIELD(arg0, f32 *, 0x2FC) * temp_f2_2);
    M2C_FIELD(arg0, f32 *, 0x300) = (f32) (M2C_FIELD(arg0, f32 *, 0x300) * temp_f2_2);
    temp_f2_3 = invmagnitude((f32 *)((u8 *)arg0+0x304)) * super_speed_scale_z;
    M2C_FIELD(arg0, f32 *, 0x304) = (f32) (M2C_FIELD(arg0, f32 *, 0x304) * temp_f2_3);
    M2C_FIELD(arg0, f32 *, 0x308) = (f32) (M2C_FIELD(arg0, f32 *, 0x308) * temp_f2_3);
    M2C_FIELD(arg0, f32 *, 0x30C) = (f32) (M2C_FIELD(arg0, f32 *, 0x30C) * temp_f2_3);
}
