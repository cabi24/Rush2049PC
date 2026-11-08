typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define TRUE 1
typedef struct { u32 opaque[6]; } OSMesgQueue;   /* 24-byte libultra queue */
extern OSMesgQueue text_lock_queue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
extern char **gText_single;
extern s32 gGameState;
extern s32 playerInPause;
typedef struct { s8 flag; s8 pad[15]; } Slot16;
extern Slot16 gControllerData[];
extern s8 audio_panesCreated;
extern s8 audio_gControllerRemoved;
extern s32 audio_gTitlePane;
extern s32 audio_menuRect[3];
extern char audio_title_player_format[];
extern s8 options_gControllerRemoved;
extern s32 options_gTitlePane;
extern char options_title_format[];
typedef struct { u32 first, second; } Pair;
extern Pair gJust;   /* defined here: the two tail stores share one lui at */
void TB_SetJust(u32 first, u32 second);
extern s8 attract_alt_text_flag;
extern char demo_alt_message[];
void TB_Sprintf(char *, char *, ...);
s32 HAL_GetTextWidth(char *, s32);
s16 TB_GetFontLineHeight(void);
void ChangePaneSize(s32, s32, s32, s32, s32);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
s32 InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
void HAL_SetTextDepth(f32);
void TB_SetTextColor(s32);
void TB_DrawTextScrollBox(s32, s32, s32, s32, s32, s32, char *);
void audio_TitlePane(void);
void options_TitlePane(void);


/* audio_animPanes = animPanes@audioscr.c (MAT3 GameCube audioscr.c animPanes, GC 0x800662D0):
 * font_set_g3d = HAL_SetFont (inlined), audio_TitlePane = _TitlePane@audioscr.c, TB_GetFontLineHeight =
 * TB_GetFontLineHeight, InitPane, audio_menuRect = menuRect {x, w, h}.
 * Disclosed: x, u1 and u2 are UNUSED locals = exact frame residual. Retail frame 120 with y at sp+116; with y
 * alone the frame is 112 (y at sp+110, 5 words differ); x (declared before y) puts y in retail's slot (sp+116)
 * and u1/u2 supply the remaining local area (with 8 bytes of locals or less the frame stays 112). */
typedef struct { s32 x, w, h; } MenuRect;
s32 HAL_SetFont(s32 fontID);
s32 audio_animPanes(s32 arg0)
{
    s16 x;
    s16 y;
    s32 u1;
    s32 u2;

    if (!audio_panesCreated) {
        if (!(gGameState & 0x7C03FFFE)) {
            HAL_SetFont(13);
            audio_gControllerRemoved = !gControllerData[playerInPause].flag;
            audio_TitlePane();
            y = 10 + 2 * TB_GetFontLineHeight();
            HAL_SetFont(10);
            InitPane(((MenuRect *)audio_menuRect)->x - 8, y - 4,
                              ((MenuRect *)audio_menuRect)->x + ((MenuRect *)audio_menuRect)->w + 8,
                              y + ((MenuRect *)audio_menuRect)->h + 4, 176, 0, 0, 0);
        }
        audio_panesCreated = 1;
    }
    return 1;
}

s32 demo_animText(s32 arg0)
{
    char buf[40];   /* unused: exact frame residual (retail frame 144, 104 without it) -- see RESULTS */

    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    if (attract_alt_text_flag == 1U) {   /* 1U shares the u32 1 of the inlined TB_SetJust(1, 1) stores */
        HAL_SetFont(13);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 10, 320, 110, -1, 0, demo_alt_message);
        TB_DrawTextScrollBox(160, 190, 320, 110, -1, 0, gText_single[33]);
    } else {
        HAL_SetFont(11);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 185, 320, 110, -1, 0, gText_single[32]);
        TB_SetTextColor(1);
    }
    gJust.second = 3;
    gJust.first = 0;
    HAL_SetTextDepth(-1.0f);
    return 1;
}
