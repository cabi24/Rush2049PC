/* mtx_rotation_to_fixed.c: a float rotation matrix into the RSP's fixed-point matrix format.
 *
 * The scene draw (zoid_draw) converts each view's camera rotation (zoid_lookat_matrix, from guLookAtF) with it into
 * frame_dynamic->look[view], which it multiplies onto the projection. */
#include "game/game.h"

/* Convert the rotation part (rows 0-2, columns 0-2) of the 4x4 float matrix `matrix` into the RSP's 4x4 s15.16
 * fixed-point Mtx at `fixed` (16 words: eight words of integer halves, then eight of fractions, each word two
 * elements). Each element is multiplied by 65536 and split: the top 16 bits go to the integer word, the low 16 to
 * the fraction word eight words on. The fourth column of the rows is zero, and the last row is (0, 0, 0, 1). */
void mtx_rotation_to_fixed(f32 matrix[4][4], u32 *fixed) {
    s32 i, first, second;
    u32 *integer = fixed;

    for (i = 0; i < 3; i++) {
        first = matrix[i][0] * 65536.0f;
        second = matrix[i][1] * 65536.0f;
        *integer++ = (first & 0xffff0000) | ((u32)second >> 16);
        integer[7] = (first << 16) | (second & 0xffff);
        first = matrix[i][2] * 65536.0f;
        *integer++ = first & 0xffff0000;
        integer[7] = first << 16;
    }
    integer[0] = 0;
    integer[8] = 0;
    integer[1] = 1;
    integer[9] = 0;
}
