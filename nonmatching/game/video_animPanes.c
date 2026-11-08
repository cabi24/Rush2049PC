/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * video_animPanes (0x8010E8B4, 340 B) = the pause/option-screen init arm of GC initScreen@optionscr.c (else-arm:
 * gControllerRemoved = !gControllerData[playerInPause], HAL_SetFont(13), _TitlePane, TB_GetFontLineHeight,
 * InitPane). sound_loop_set (= _TitlePane) is defined in this file too (body).
 * DRAFT: unit 3/85 words with `the whole-program unit. --internal sound_loop_set` (was 21/85, frame 96/80).
 * Levers that worked: the controller byte read into a named s8 `c` (retail keeps it in v0: 21 -> 7 incl. frame),
 * `video_menuRect[1] + video_menuRect[0] + 8` operand order (a2 = v1 + t6 as retail), `h` (s32) holding the
 * unnarrowed line-height expression.
 * Frame: retail 96; `s32 u0` is an UNUSED local = frame residual (80 without it and h; 96 with h + u0) -- not
 * proven to be the real source; disclose if kept.
 * Residual (3 words): retail narrows the line height through ring temps (`sll t8,t0; sra t9,t8`, uses t9) and
 * keeps the unnarrowed value in t0; ours narrows in place (`sra t0,t8`, ugen `sra $25; move $8,$25` merged by
 * as1). Same family residual as the `x - (w>>1)` TB_DrawText narrowing in animTitleText/B7FC/BC84.
 * PC (compiled into the PC build, CMakeLists.txt): animPanes@videoscr.c, the AnimFunc of the video-options screen's
 * MultiBlit tables video_frontEndBlit (front end) and video_pauseBlit (in game), which video_initScreen hands to NewMultiBlit
 * (ROM data). NewMultiBlit calls it once with the new blit, DrawBlits every frame after. No direct callers.
 * InitPane takes its sixth argument pointer-sized on the PC.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

extern OSMesgQueue text_lock_queue;
extern s8 video_panesCreated;
extern u32 gGameState;
extern s8 video_gControllerRemoved;
extern s32 playerInPause;
extern s8 gControllerData[][16];
extern s32 video_menuRect[3];
extern void *video_gTitlePane;

typedef struct TblHdr {
    u8 pad0[0xD4];
    char *title;
    u8 padD8[0x3A4 - 0xD8];
    char *ctrl;
} TblHdr;

extern TblHdr *gText_single;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFont(s32 fontID);
void video_TitlePane(void);
void TB_Sprintf(char *, char *, ...);
u32 HAL_GetTextWidth(void *, s32);
void ChangePaneSize(void *, s32, s32, s32, s32);
s16 TB_GetFontLineHeight(void);
void *InitPane(s32 x1, s32 y1, s32 x2, s32 y2, s32 style, intptr_t word40, s32 word44, s32 own);

s32 video_animPanes(void *item)
{
    s32 u0;
    s16 y;
    s32 h;
    s8 c;

    if (!video_panesCreated) {
        if (!(gGameState & 0x7C03FFFE)) {
            HAL_SetFont(13);
            c = gControllerData[playerInPause][0];
            video_gControllerRemoved = c == 0;
            video_TitlePane();
            h = TB_GetFontLineHeight() * 2 + 10;
            y = h;
            InitPane(video_menuRect[0] - 8, y - 4, video_menuRect[1] + video_menuRect[0] + 8, y + video_menuRect[2] + 4,
                              176, 0, 0, 0);
        }
        InitPane(20, 136, 300, 204, 176, 0, 0, 0);
        video_panesCreated = 1;
    }
    return 1;
}
