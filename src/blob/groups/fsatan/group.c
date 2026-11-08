/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Match recipe: compile as a genuine call group, keep fsatan external,
 * leave fxatan internal via uld -kp. See group.json and STATUS.md.
 * C680: piecewise argument reduction around a rational approximation.
 * C5E0: real sole callee; reconstructed from locked src/blob/fxatan.c.
 * No coefficient values are guessed; all are the original external f32 data.
 */
typedef float f32;
extern f32 atan_p0;
extern f32 atan_p4;
extern f32 atan_p3;
extern f32 atan_p2;
extern f32 atan_p1;
extern f32 atan_q4;
extern f32 atan_q3;
extern f32 atan_q2;
extern f32 atan_q1;
extern f32 atan_sq2m1;
extern f32 atan_sq2p1;
extern f32 atan_pio2;
extern f32 atan_pio4;

f32 fxatan(f32 arg0);

f32 fsatan(f32 arg0) {
    if (arg0 < atan_sq2m1) {
        return fxatan(arg0);
    }
    if (atan_sq2p1 < arg0) {
        return atan_pio2 - fxatan(1.0f / arg0);
    }
    return fxatan((arg0 - 1.0f) / (arg0 + 1.0f)) + atan_pio4;
}
