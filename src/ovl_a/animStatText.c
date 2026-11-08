/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Runtime image A: nested menu-row callback, complete 428-byte body.
 * N64-specific; direct arcade equivalent not identified.
 * The incoming callback value is never read: its parameter is reused as the
 * outer index rather than giving that index a separate automatic home.
 * Two unused s32 declarations represent the measured frame residual only:
 * unshaped parameter-reuse frame 104 B, native 112 B, two 4-byte slots.
 * The first precedes the live locals; the second follows them. This retains
 * the real signed-halfword index at sp+102 and cached flag pointer at sp+76.
 * These are disclosed shaping declarations, not recovered original names.
 * All live buffer/type capacities and helper contracts are unchanged.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef float f32;
typedef struct Texture { u8 pad0[18]; u16 height; u8 pad20[16]; } Texture;
typedef struct MenuIndices { u8 pad0[26]; u16 first; } MenuIndices;
/* PC: gText's header has host pointers (port/assets/text.c), so its fields sit at host offsets */
typedef struct MenuData {
    void *pad0[3];
    MenuIndices *indices;
    char **text;
} MenuData;
extern s16 gNumPlayers;
extern s32 gRace;
extern s8 gCarLocked[];
extern volatile u8 gNumModels;
extern char carscr_stat_text_texture_name[];
extern MenuData gText;
void HAL_SetTextDepth(f32);
void *HAL_SetFont(s32);
void TB_SetTextColor(s32);
Texture *MBOX_FindTexture_WORK(char *, s16 *, s8, s8, s32);
s16 TB_GetFontLineHeight(void);
void TB_DrawText(s16, s16, void *);

s32 animStatText(s32 i)
{
    s32 frame_slot0;
    s32 j;
    s16 index;
    s32 y;
    Texture *texture;
    s32 frame_slot1;
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(12);
    TB_SetTextColor(1);
    for (i = 0; i < gNumPlayers; i++) {
        if (gCarLocked[i] == 1) {
            continue;
        }
        for (j = 0; j < 4; j++) {
            if (i < gNumPlayers && gNumPlayers == 1 && gRace != 2) {
                texture = MBOX_FindTexture_WORK(carscr_stat_text_texture_name, &index, 0, gNumModels - 1, 1);
                y = 48 + 2 * j * (texture->height / 8);
                TB_DrawText(10, y - TB_GetFontLineHeight(), gText.text[gText.indices->first + j]);
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
