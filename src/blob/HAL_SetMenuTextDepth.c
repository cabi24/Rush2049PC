/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * HAL_SetMenuTextDepth @ 0x800F6928, 400 bytes: piecewise-linear table lookup, then publish the result.
 * menu_text_depth_curve is a table of (x, y) float pairs terminated by an x >= 32000.0f sentinel. Find the
 * first entry whose x is not below `value` (or the sentinel); when it is not the first entry,
 * linearly interpolate y between the previous and that entry. The tail is the same code as the
 * locked HAL_SetTextDepth (store gDepth; when positive set bit 0x10 of gFontAttribute and pass the
 * value, converted f32 -> u32 -> u16, to BlitZDist, otherwise clear the bit). No arcade
 * ancestor identified.
 *
 * Strict MATCH at -O3 and at -O2 (single file), EQUAL in the whole-program unit (the whole-program unit).
 * No own rodata (32000.0f is an lui immediate, 0.0f is mtc1 zero).
 *
 * Shaping facts (each needed):
 * - the three differences are named locals `a, b, d` assigned in the order d, b, a: this puts them
 *   in $f18/$f16/$f14 as in retail (written inline they become ugen temps in other registers);
 * - the final step is two statements, `value = a * b / d; value += y0;`: one expression gives
 *   `add.s y0, q` where retail has `add.s q, y0`;
 * - the plain `while` loop with `i = 0` reproduces retail's unfolded `sll t6,zero,3` address.
 */
typedef float f32;
typedef int s32;
typedef unsigned int u32;
typedef unsigned short u16;
typedef struct { f32 x, y; } Point;
extern Point menu_text_depth_curve[];
extern f32 gDepth;
extern u32 gFontAttribute;
extern void BlitZDist(u16);

void HAL_SetMenuTextDepth(f32 value) {
    s32 i;
    f32 a, b, d;

    i = 0;
    while (value > menu_text_depth_curve[i].x && menu_text_depth_curve[i].x < 32000.0f) {
        i++;
    }
    if (i > 0) {
        d = menu_text_depth_curve[i].x - menu_text_depth_curve[i - 1].x;
        b = menu_text_depth_curve[i].y - menu_text_depth_curve[i - 1].y;
        a = value - menu_text_depth_curve[i - 1].x;
        value = a * b / d;
        value += menu_text_depth_curve[i - 1].y;
    }
    gDepth = value;
    if (value > 0.0f) {
        gFontAttribute |= 0x10;
        BlitZDist(value);
    } else {
        gFontAttribute &= ~0x10;
    }
}
