/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ZOID_SplitMenuScreen (0x800A6BE4, 2072 B): historical label. Twin of ZOID_SplitScreen (0x800A6404): the
 * same N64 split-screen viewport layout, except the 2-player layout uses two 3/4-width panes starting at
 * x = 2 (width 3W/4 + 2, centre 3W/8 + 2) with the unscaled fov. Cases 1 and 3/4 are identical to
 * ZOID_SplitScreen (0.95f quarter fov). No arcade counterpart.
 *
 * Source-form notes: as ZOID_SplitScreen (helper here view_bind_w, non-static, force_internal in the unit) (inlined static view_bind whose unused return value is the dead
 * v0 copy before each next call; block-scoped u16 rect locals; per-pane store order). The constant 3 is
 * one coloured web here (n == 3 compare, pane index 3, the W * 3 / H * 3 multiplies: multu by s4).
 * The second 2-player pane stores h, x, w, y (as1 order). No unused locals.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;

typedef struct {
    void *f0;
    void *f4;
    u8 unk8[0x40];
} ArbEntry;

extern u8 gNumZoidWindows;
extern s8 gSplitScreen;
extern s32 screen_width, screen_height;
extern f32 HFOV;
/* PC: the windows' viewports are Vp records (s16 scale[4], translate[4]; window_init's Viewport16). The PC loader
 * fills initialized data by its declarations: as s32 the first two halves would be swapped together and the rest
 * left big-endian (docs/port/ENDIAN.md). */
typedef struct { s16 scale[4], translate[4]; } PcViewport;
extern PcViewport vp0, vp1, vp2, vpul, vpur, vpll, vplr;
extern s16 scissor0[4], scissor1[4], scissor2[4], scissorul[4], scissorur[4], scissorll[4], scissorlr[4];
extern ArbEntry ZoidWinInfo[];

ArbEntry *view_bind_w(s32 k, void *vp, s16 *r) {
    ZoidWinInfo[k].f0 = vp;
    ZoidWinInfo[k].f4 = r;
    return &ZoidWinInfo[k];
}

ArbEntry *ZOID_InitializeWindow(s32 idx, void *a, void *b, f32 c, f32 d, f32 e, f32 f, f32 g, f32 h);

s32 ZOID_SplitMenuScreen(s32 n) {
    if (n > 0) {
        gNumZoidWindows = n;
        if (n == 1) {
            ZOID_InitializeWindow(0, &vp0, scissor0, HFOV, 0.0f, (f32)screen_width, (f32)screen_height,
                         (f32)(screen_width / 2), (f32)(screen_height / 2));
            {
                u16 w, h;
                w = screen_width; h = screen_height;
                scissor0[3] = h;
                scissor0[2] = w;
                scissor0[0] = 0;
                scissor0[1] = 0;
                view_bind_w(0, &vp0, scissor0);
            }
            gSplitScreen = 0;
        } else if (n == 2) {
            ZOID_InitializeWindow(0, &vp1, scissor1, HFOV, 0.0f,
                         (f32)(screen_width * 3 / 4), (f32)(screen_height / 2), (f32)(screen_width * 3 / 8 + 2), (f32)(screen_height / 4));
            {
                u16 w, h;
                w = screen_width * 3 / 4 + 2; h = screen_height / 2 - 1;
                scissor1[3] = h;
                scissor1[2] = w;
                scissor1[0] = 2;
                scissor1[1] = 0;
                view_bind_w(0, &vp1, scissor1);
            }
            ZOID_InitializeWindow(1, &vp2, scissor2, HFOV, 0.0f,
                         (f32)(screen_width * 3 / 4), (f32)(screen_height / 2), (f32)(screen_width * 3 / 8 + 2), (f32)(screen_height * 3 / 4));
            {
                u16 y, w, h;
                y = screen_height / 2 + 1; w = screen_width * 3 / 4 + 2; h = screen_height;
                scissor2[3] = h;
                scissor2[0] = 2;
                scissor2[2] = w;
                scissor2[1] = y;
                view_bind_w(1, &vp2, scissor2);
            }
            gSplitScreen = 1;
        } else if (n == 3 || n == 4) {
            ZOID_InitializeWindow(0, &vpul, scissorul, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2), (f32)(screen_width / 4 + 1), (f32)(screen_height / 4 + 1));
            {
                u16 w, h;
                w = screen_width / 2 - 1; h = screen_height / 2 - 1;
                scissorul[3] = h;
                scissorul[2] = w;
                scissorul[0] = 0;
                scissorul[1] = 0;
                view_bind_w(0, &vpul, scissorul);
            }
            ZOID_InitializeWindow(1, &vpur, scissorur, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2), (f32)(screen_width * 3 / 4 - 2), (f32)(screen_height / 4 + 1));
            {
                u16 x, w, h;
                x = screen_width / 2 + 1; w = screen_width; h = screen_height / 2 - 1;
                scissorur[3] = h;
                scissorur[2] = w;
                scissorur[0] = x;
                scissorur[1] = 0;
                view_bind_w(1, &vpur, scissorur);
            }
            ZOID_InitializeWindow(2, &vpll, scissorll, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2 + 2), (f32)(screen_width / 4 + 1), (f32)(screen_height * 3 / 4 + 2));
            {
                u16 y, w, h;
                y = screen_height / 2 + 1; w = screen_width / 2 - 1; h = screen_height;
                scissorll[3] = h;
                scissorll[2] = w;
                scissorll[1] = y;
                scissorll[0] = 0;
                view_bind_w(2, &vpll, scissorll);
            }
            ZOID_InitializeWindow(3, &vplr, scissorlr, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2 + 2), (f32)(screen_width * 3 / 4 - 2), (f32)(screen_height * 3 / 4 + 2));
            {
                u16 x, y, w, h;
                x = screen_width / 2 + 1; y = screen_height / 2 + 1; w = screen_width; h = screen_height;
                scissorlr[3] = h;
                scissorlr[2] = w;
                scissorlr[1] = y;
                scissorlr[0] = x;
                view_bind_w(3, &vplr, scissorlr);
            }
            gSplitScreen = 2;
        }
    }
    return gSplitScreen;
}
