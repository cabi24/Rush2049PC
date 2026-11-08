/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * sound_loop_set (0x800B574C, 332 B) = GC _TitlePane@optionscr.c (GC locals width, str1[64]; N64 adds height and
 * x = width/2, all used). Option/pause screen title pane: font 13; "CONTROLLER %d %s" (playerInPause + 1, StrTable
 * +0x3A4) when gControllerRemoved (video_gControllerRemoved), else the screen title (StrTable +0xD4); the pane gTitlePane
 * (video_gTitlePane) is resized (ChangePaneSize) or created (InitPane).
 * Body: . Internal function (unsaved s0,s1,s2,s4: its callers updateVideoScreen and video_animPanes save them).
 * Shaping (disclosed):
 *  - the HUD family's inlined lock/font helpers (gfx_lock/gfx_unlock/font_select/font_set) as non-static,
 * uniquely named functions not on keep (static ones leave symbol-less stubs after video_uninitScreen).
 *  - the four helpers are written on ONE source line. With a single inlined call site umerge keeps the helpers'
 *    .loc lines and as1 then breaks the jal-osSendMesg delay-slot tie the other way (`move s0,v0` instead of
 *    `move a2,zero`, 4 words). In the unit with video_animPanes also C (two call sites of font_set) the inlined
 * code carries no.loc and matches either way. One line = no line-number difference = retail order.
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

/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct TblHdr {
    char *pad0[0xD4 / 4];
    char *title;
    char *padD8[(0x3A4 - 0xD8) / 4];
    char *ctrl;
} TblHdr;

extern TblHdr *gText_single;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void TB_Sprintf(char *, char *, ...);
u32 HAL_GetTextWidth(void *, s32);
void ChangePaneSize(void *, s32, s32, s32, s32);
s16 TB_GetFontLineHeight(void);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);

void text_lock(void);
void text_unlock(void);
   s32 font_set_slp(s32 font) { s32 old; text_lock(); old = HAL_SetFontInternal(font); text_unlock(); return old; }

void video_TitlePane(void)
{
    s32 width;
    s32 height;
    s32 x;
    char str1[64];

    font_set_slp(13);
    if (video_gControllerRemoved) {
        TB_Sprintf(str1, "CONTROLLER %d %s", playerInPause + 1, gText_single->ctrl);
        width = HAL_GetTextWidth(str1, -1);
    } else {
        width = HAL_GetTextWidth(gText_single->title, -1);
    }
    if (video_gTitlePane) {
        height = TB_GetFontLineHeight();
        x = width / 2;
        ChangePaneSize(video_gTitlePane, 160 - x - 8, 6, x + 168, height + 14);
    } else {
        height = TB_GetFontLineHeight();
        x = width / 2;
        video_gTitlePane = InitPane(160 - x - 8, 6, x + 168, height + 14, 176, 0, 0, 0);
    }
}
