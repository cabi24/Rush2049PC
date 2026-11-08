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
/* The text tables (port/assets/text.c fills them with host pointers): single, the strings by index. */
typedef struct TextTables {
    unsigned short numSingle; unsigned char **single;
    unsigned short numTab, numTabA; unsigned short *tab; unsigned char **taba;
    unsigned short numTab2, numTab2a, numTab2b; unsigned short *tab2; unsigned short *tab2a; unsigned char **tab2b;
} TextTables;
extern TextTables gText;
extern s32 gGameState;
extern s32 playerInPause;
typedef struct { s8 flag; s8 pad[15]; } Slot16;
extern Slot16 gControllerData[];
extern s8 audio_panesCreated;
extern s8 audio_gControllerRemoved;
extern intptr_t audio_gTitlePane;
extern s32 audio_menuRect[3];
extern char audio_title_player_format[];
extern s8 options_gControllerRemoved;
extern intptr_t options_gTitlePane;
extern char options_title_format[];
typedef struct { u32 first, second; } Pair;
extern Pair gJust;
void TB_SetJust(u32 first, u32 second);
extern s8 attract_alt_text_flag;
extern char demo_alt_message[];
void TB_Sprintf(char *, char *, ...);
s32 HAL_GetTextWidth(char *, s32);
s16 TB_GetFontLineHeight(void);
void ChangePaneSize(intptr_t, s32, s32, s32, s32);
intptr_t InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
void HAL_SetTextDepth(f32);
void TB_SetTextColor(s32);
void TB_DrawTextScrollBox(s32, s32, s32, s32, s32, s32, char *);
void audio_TitlePane(void);
void options_TitlePane(void);


s32 HAL_SetFont(s32 fontID);
void audio_TitlePane(void)
{
    char buf[76];
    s32 w;

    HAL_SetFont(13);
    if (audio_gControllerRemoved) {
        TB_Sprintf(buf, audio_title_player_format, playerInPause + 1, gText.single[233]);
        w = HAL_GetTextWidth(buf, -1);
    } else {
        w = HAL_GetTextWidth(gText.single[52], -1);
    }
    if (audio_gTitlePane) {
        ChangePaneSize(audio_gTitlePane, 152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14);
    } else {
        audio_gTitlePane = InitPane(152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
    }
}


void options_TitlePane(void)
{
    char buf[76];
    s32 w;

    HAL_SetFont(13);
    if (options_gControllerRemoved) {
        TB_Sprintf(buf, options_title_format, playerInPause + 1, gText.single[233]);
        w = HAL_GetTextWidth(buf, -1);
    } else {
        w = HAL_GetTextWidth(gText.single[50], -1);
    }
    if (options_gTitlePane) {
        ChangePaneSize(options_gTitlePane, 152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14);
    } else {
        options_gTitlePane = InitPane(152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
    }
}

