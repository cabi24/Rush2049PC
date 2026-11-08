/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ZOID_ResetWindows (0x800A5A40, 252 B) -- full-screen viewport 0 setup.
 * Calls ZOID_InitializeWindow(0, ...) with the screen size screen_width/screen_height (w, h), their halves
 * and 0.0f, then re-binds entry 0 of ZoidWinInfo (stride 0x48) to &vp0 and the s16 rect
 * scissor0 = {0, 0, (u16)w, (u16)h}. N64-specific (no arcade counterpart found).
 *
 * Shaping devices (disclosed):
 *  - ZOID_InitializeWindow returns its entry pointer (retail ZOID_InitializeWindow reloads &ZoidWinInfo[idx] into v0
 *    before `jr ra`), so it is declared returning ArbEntry *; the result is kept in `e` and read by a
 *    compiled-out `if (e) {}` at the end. That keeps v0 busy (h/w colour a0/v1 as retail) and ends the
 *    block before the epilogue (retail `lw ra` after the stores, `jr ra; nop`).
 *  - `w = ...; h = ...;` on ONE source line: as1 breaks aftercycles ties by .loc line, and retail's
 *    lui/lhu interleave (lui v1, lui a0, lhu a0, lhu v1) needs both loads on the same line.
 * Unit: the whole-program unit score ZOID_ResetWindows --neighbours -> EQUAL, 0 locked bodies differ.
 */
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;

typedef struct {
    void *f0;
    void *f4;
    u8 unk8[0x40]; /* fields 0x08-0x47 set by ZOID_InitializeWindow */
} ArbEntry;

extern s32 screen_width;
extern s32 screen_height;
extern u8 gNumZoidWindows;
extern u8 gSplitScreen;
extern s32 vp0;
extern s16 scissor0[4];
extern f32 HFOV;   /* the field of view (90.0f): ZOID_InitializeWindow takes it as an f32 */
extern ArbEntry ZoidWinInfo[];

ArbEntry *ZOID_InitializeWindow(s32 idx, void *a, void *b, f32 c, f32 d, f32 e, f32 f, f32 g, f32 h);

void ZOID_ResetWindows(void) {
    ArbEntry *e;
    u16 h;
    u16 w;

    gNumZoidWindows = 1;
    gSplitScreen = 0;
    e = ZOID_InitializeWindow(0, &vp0, scissor0, HFOV, 0.0f, (f32)screen_width, (f32)screen_height,
                 (f32)(screen_width / 2), (f32)(screen_height / 2));
    w = screen_width; h = screen_height;
    scissor0[0] = 0;
    scissor0[1] = 0;
    ZoidWinInfo[0].f0 = &vp0;
    ZoidWinInfo[0].f4 = scissor0;
    scissor0[3] = h;
    scissor0[2] = w;
    if (e) {}
}
