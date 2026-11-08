/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef float f32;
f32 dot_uv_y_axis(f32 *a,f32 *b) {
    return (a[0]*b[3]+a[1]*b[4])+a[2]*b[5];
}
