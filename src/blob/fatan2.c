/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (single file; -O2 leaves 22 words: y in $f20 instead of $f16) */
typedef float f32;

extern f32 fsatan(f32);
extern f32 fatan(f32);
extern const f32 atan2_pio2;
extern const f32 atan2_neg_pio2;
extern const f32 atan2_pi;
extern const f32 atan2_neg_pi;

f32 fatan2(f32 y, f32 x) {
    if (y == y + x) {
        if (y == 0.0f) {
            return 0;
        }
        if (y > 0.0f) {
            return atan2_pio2;
        }
        return atan2_neg_pio2;
    }
    if (x < 0.0f) {
        if (y >= 0.0f) {
            return atan2_pi - fsatan(-y / x);
        }
        return fsatan(y / x) + atan2_neg_pi;
    }
    if (y > 0) {
        return fatan(y / x);
    }
    return -fatan(-y / x);
}
