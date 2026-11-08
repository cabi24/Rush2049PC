/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef short s16;typedef signed char s8;typedef unsigned char u8;typedef int s32;typedef float f32;
#define M2C_FIELD(p,t,o) (*(t)((u8*)(p)+(o)))
extern f32 fixed_cam_zoom_rate;extern f32 fixed_cam_pos[][3];extern s8 fixed_cam_mode[];extern f32 elastic_factor[];
extern f32 vector_magnitude(f32 *);extern void point_at_car(void*,void*,intptr_t,f32*);
void fix_camera_in_space(s16 arg0, void *arg1, void *arg2, intptr_t arg3) {
    f32 vector[3];
    f32 temp_f0;
    f32 temp_f2;
    s8 temp_v1;
    f32 *temp_v0;

    temp_v1 = M2C_FIELD(arg1, s8 *, CAR_OFS(0x35C));
    if (arg0 == 0) {
        fixed_cam_mode[temp_v1] = 0;
        temp_v0 = fixed_cam_pos[temp_v1];
        M2C_FIELD(temp_v0, f32 *, 0) = (f32) M2C_FIELD(arg2, f32 *, 0);
        M2C_FIELD(temp_v0, f32 *, 4) = (f32) M2C_FIELD(arg2, f32 *, 4);
        M2C_FIELD(temp_v0, f32 *, 8) = (f32) M2C_FIELD(arg2, f32 *, 8);
        return;
    }
    if (arg0 == 1) {
        fixed_cam_mode[temp_v1] = 1;
        temp_v0 = fixed_cam_pos[temp_v1];
        M2C_FIELD(temp_v0, f32 *, 0) = (f32) M2C_FIELD(arg2, f32 *, 0);
        M2C_FIELD(temp_v0, f32 *, 4) = (f32) M2C_FIELD(arg2, f32 *, 4);
        M2C_FIELD(temp_v0, f32 *, 8) = (f32) M2C_FIELD(arg2, f32 *, 8);
        return;
    }
    temp_v0 = fixed_cam_pos[temp_v1];
    vector[0] = M2C_FIELD(temp_v0, f32 *, 0) - M2C_FIELD(arg2, f32 *, 0);
    vector[1] = M2C_FIELD(temp_v0, f32 *, 4) - M2C_FIELD(arg2, f32 *, 4);
    vector[2] = M2C_FIELD(temp_v0, f32 *, 8) - M2C_FIELD(arg2, f32 *, 8);
    elastic_factor[temp_v1] = 0.0f;
    if (fixed_cam_mode[temp_v1] != 0) {
        temp_f0 = vector_magnitude(vector);
        if (temp_f0 != 0.0f) {
            temp_f2 = (((temp_f0 - 50.0f) * fixed_cam_zoom_rate) + 20.0f) / temp_f0;
            vector[0] *= temp_f2;
            vector[1] *= temp_f2;
            vector[2] *= temp_f2;
        }
    }
    point_at_car(arg1, arg2, arg3, vector);
}

extern f32 sqrtf(f32);
#pragma intrinsic (sqrtf)
f32 vector_magnitude(f32 *v);
