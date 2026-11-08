/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* calcControlMenuMaxSize = calcControlMenuMaxSize (controlscr.c; map: proven). Direct typed port of MAT3
 * controlscr.c calcControlMenuMaxSize (GC 0x8006CD08; DWARF locals rectHeight, i, gControlMenuCount).
 * N64: twelve options (GC thirteen); validOption = controls_validOption (inlined; the unrolled loop compares i with
 * 8/7/6/5); HAL_SetFont(11) = lock / HAL_SetFontInternal / unlock, written as non-static helpers (gfx_lock_CS,
 * gfx_unlock_CS, font_set_CS: inlined, internal, deleted stubs carry symbols) defined AFTER this
 * function (defined before it, as1 schedules `lw s4` into the osRecvMesg delay slot in the group compile).
 * Internal in retail and here: it uses s2/s4/s5 without saving; updateControlScreen's colouring depends on that
 * clobber set (without this body in the unit D91A0 is 51+ words off).
 * `rectHeight -= 24;` gives retail's in-place s4 update.
 * Shaping (disclosed, both compiled out, no code): `if (gNumWindows) {}` in the loop keeps the &gNumWindows
 * address web live through it (retail's loop constants start at a3), and `do {} while (0);` after the loop
 * (block count: i takes a0 and the count a1). Found by a 86-variant holder sweep (sw/, sw2); the
 * force.sh oracle had shown the residual was colour-only. No unused locals.
 */
typedef signed short s16;
typedef signed char s8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { s32 x, y, w, h, cx, cy; } PlayerRect;        /* 0x18 */
extern PlayerRect controls_gPlayerRects[4][4];      /* gPlayerRects */
extern s16 gNumWindows;                   /* gNumWindows */
extern s16 gControlMenuSize;                   /* gControlMenuSize */
extern u32 gGameState;                 /* gGameState */
typedef struct { u32 opaque[6]; } OSMesgQueue;
extern OSMesgQueue text_lock_queue;           /* text/gfx mutex */
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 font);          /* font select core (IPA s2 parameter) */

void gfx_lock_CS(void);
void gfx_unlock_CS(void);
s32 font_set_CS(s32 font);
s32 controls_validOption(s8, s32);              /* validOption */

void calcControlMenuMaxSize(void) {
    s32 rectHeight = controls_gPlayerRects[gNumWindows - 1][0].h;
    s32 i;
    s32 gControlMenuCount;
    font_set_CS(11);
    rectHeight -= 24;
    gControlMenuSize = rectHeight / 16;
    gControlMenuCount = 2;
    for (i = 0; i < 12; i++) {
        if (gNumWindows) {}
        if (controls_validOption(0, i)) gControlMenuCount++;
    }
    do {} while (0);
    if (gControlMenuSize > gControlMenuCount || (gGameState & 0x7C03FFFE))
        gControlMenuSize = gControlMenuCount;
}

void gfx_lock_CS(void) { osRecvMesg(&text_lock_queue, (void *)0, 1); }
void gfx_unlock_CS(void) { osSendMesg(&text_lock_queue, (void *)0, 0); }
s32 font_set_CS(s32 font)                /* HAL_SetFont */
{
    s32 old;

    gfx_lock_CS();
    old = HAL_SetFontInternal(font);
    gfx_unlock_CS();
    return old;
}
