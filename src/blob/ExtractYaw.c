/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef float f32;
extern const f32 extract_yaw_epsilon;
extern f32 fatan2(f32,f32);
extern f32 fabsf(f32);
#pragma intrinsic (fabsf)
#define C24_ABS(x) fabsf(x)
void ExtractYaw(f32 *m, f32 *out) {
 f32 x=m[6];
 if (C24_ABS(x)<extract_yaw_epsilon && C24_ABS(m[8])<extract_yaw_epsilon) {
  *out=fatan2(m[2],m[0]);
 } else {
  *out=fatan2(-x,m[8]);
 }
}
