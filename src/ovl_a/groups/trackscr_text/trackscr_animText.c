/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image A:803AECE4..803AF744, complete projected settings-label/value callback.
 * This callback is MAT3 trackscr.c animText; its body is animButtonText, now the non-kept
 * helper trackscr_animButtonText (inlined; its deleted procedure is the retail stub 0x803AECDC right before animText).
 * umerge's 8-byte area for that inlined call sits exactly where the native x/y homes were (retail never
 * touches sp+208..215; i is the memory-resident counter at sp+204), so the helper declares i first and x, y
 * last, and the native `slot` carrier is gone (trackscr_buttonInfo[index] directly). N64 animText has no
 * `else drawOptions()` (a second inlined call would add 8 more bytes). Frame 216, buffer capacities unchanged. Native expressions
 * evaluate the selected table base before the language offset; three existing
 * label assignments preserve text evaluation before argument narrowing.
 * Reordered existing vector writes and a split Y subtraction preserve the
 * native named value and floating temporary sequence. No padding, artificial
 * read, volatile, fake caller, extra parameter or compiler patch is used.
 * Color4 is the word-aligned native view of the two four-byte by-value colors.
 * Six natural angle literals and the 21-entry switch table own 108 verified
 * bytes at 803B99E0..803B9A4C. No exact arcade donor is claimed. */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct Color4 { u32 word; } Color4;
typedef struct Language { s32 unknown0; char **text; s32 unknown8; u16 *indices; char **indexed; } Language;
typedef struct Slot { f32 x,y,angle; u8 unknown0C[36]; f32 position[3]; u8 alpha,unknown3D[3]; } Slot;
typedef struct Camera { f32 uv[3][3],position[3]; u8 unknown30[104]; } Camera;
typedef struct Settings { u8 unknown00[28]; s8 mode; u8 unknown1D[3]; s8 a,b,c,d,e,f,g,h,i; } Settings;
extern s32 trackscr_buttonsInited,gRace;
extern f32 trackscr_title_button_position[3],trackscr_gButtonSpace,trackscr_gButtonScale;
extern s8 expansion_pak_present,gTrackNum,gGhostSelect,gBackwards,gCircuitTrack,gDeathMode;
extern s16 gNumLaps,gNumDrones,trackscr_gNumOptions,trackscr_gOpts[];
extern Slot trackscr_buttonInfo[];
extern Camera gCamera[];
extern Language gText;
extern Color4 flashColors,flashColors_at_1;
extern char *gTrackTitles[];
extern Settings gOptions;
extern u8 gCircuitTracks[][5];   /* the circuit's tracks (trackscr.c's CircuitTrack): [2] the mirror flag */
extern char trackscr_laps_format[],trackscr_option5_format[],trackscr_option6_format[],trackscr_option7_format[],trackscr_option9_format[],trackscr_option10_format[],trackscr_drones_format[];
extern void HAL_SetTextDepth(f32);
extern s32 HAL_SetFont(s32);
extern void TB_SetTextColor(s32);
extern void TB_SetJust(u32,u32);
extern void ZOID_ProjectWindow(s32,f32 *,Camera *,void *,s16 *);
extern s16 TB_CenterText(void *,s16);
extern void TB_DrawText(s16,s16,void *);
extern void TB_Sprintf(char *,char *,...);
extern int sprintf(char *,const char *,...);
extern void HAL_SetTextTranslucency(f32);
extern void TB_FlashTextColors(Color4,Color4);

void trackscr_animButtonText(void);
s32 trackscr_animText(void *callback_context)
{
    if (trackscr_buttonsInited == 1) trackscr_animButtonText();
    return 1;
}

/* animButtonText@trackscr.c (MAT3): inlined into animText trackscr_animText; deleted stub 0x803AECDC */
void trackscr_animButtonText(void)
{
    s32 i;
    char text[80];
    char *label;
    s16 point[2];
    f32 position[3];
    s32 index;
    s32 x;
    s32 y;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(0, 1);
    ZOID_ProjectWindow(0, trackscr_title_button_position, &gCamera[0], 0, point);
    x = point[0];
    y = point[1];
    HAL_SetFont(13);
    TB_SetTextColor(1);
    TB_Sprintf(text, gText.text[45]);
    TB_DrawText(TB_CenterText(text, x), y, text);
    if (!expansion_pak_present && gTrackNum == 5) {
        position[2] = trackscr_buttonInfo[0].position[2];
        position[0] = 0.0f;
        position[1] = trackscr_buttonInfo[0].position[1];
        position[1] -= trackscr_gButtonSpace * trackscr_gButtonScale;
        HAL_SetFont(11);
        ZOID_ProjectWindow(0, position, &gCamera[0], 0, point);
        x = point[0];
        y = point[1];
        label = gText.text[232];
        TB_SetTextColor(22);
        TB_DrawText(TB_CenterText(label, x), y, label);
    }
    HAL_SetFont(11);
    for (i = 0; i < trackscr_gNumOptions; i++) {
        index = trackscr_gOpts[i];
        if ((trackscr_buttonInfo[index].angle <= 0.6981316804885864f || trackscr_buttonInfo[index].angle >= 2.4434609413146973f) && trackscr_buttonInfo[index].alpha) {
            HAL_SetTextTranslucency(trackscr_buttonInfo[index].alpha);
            ZOID_ProjectWindow(0, trackscr_buttonInfo[index].position, &gCamera[0], 0, point);
            x = point[0] - 35.0f * trackscr_gButtonScale;
            y = point[1];
            if (trackscr_buttonInfo[index].angle < 1.5707963705062866f) {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(22);
            }
            label = (gText.indexed + trackscr_gOpts[i])[gText.indices[1]];
            TB_DrawText(x, y, label);
        }
        if ((trackscr_buttonInfo[index + 21].angle <= 0.6981316804885864f || trackscr_buttonInfo[index + 21].angle >= 2.4434609413146973f) && trackscr_buttonInfo[index + 21].alpha) {
            HAL_SetTextTranslucency(trackscr_buttonInfo[index + 21].alpha);
            ZOID_ProjectWindow(0, trackscr_buttonInfo[index + 21].position, &gCamera[0], 0, point);
            x = point[0] - 35.0f * trackscr_gButtonScale;
            y = point[1];
            if (trackscr_buttonInfo[index + 21].angle < 1.5707963705062866f) {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(22);
            }
            switch (index) {
            case 0:
                label = gTrackTitles[gTrackNum];
                TB_DrawText(x, y, label);
                break;
            case 1:
                label = gText.indexed[gText.indices[35] + gGhostSelect];
                TB_DrawText(x, y, label);
                break;
            case 2:
                sprintf(text, trackscr_laps_format, gNumLaps);
                TB_DrawText(x, y, text);
                break;
            case 3:
                if (gOptions.a == 0) label = gText.text[208];
                else if (gOptions.a == 1) label = gText.text[130];
                else label = gText.text[12];
                TB_DrawText(x, y, label);
                break;
            case 4:
                TB_DrawText(x, y, (gText.indexed + (gOptions.b == 0 ? 0 : 1))[gText.indices[36]]);
                break;
            case 5:
                sprintf(text, trackscr_option5_format, gOptions.c);
                TB_DrawText(x, y, text);
                break;
            case 6:
                sprintf(text, trackscr_option6_format, gOptions.d);
                TB_DrawText(x, y, text);
                break;
            case 7:
                sprintf(text, trackscr_option7_format, gOptions.e);
                TB_DrawText(x, y, text);
                break;
            case 8:
                TB_DrawText(x, y, gOptions.f == 1 ? gText.text[208] : gText.text[130]);
                break;
            case 9:
                sprintf(text, trackscr_option9_format, gOptions.g);
                TB_DrawText(x, y, text);
                break;
            case 10:
                sprintf(text, trackscr_option10_format, gOptions.h);
                TB_DrawText(x, y, text);
                break;
            case 11:
                TB_DrawText(x, y, (gText.indexed + (gOptions.i == 0 ? 0 : 1))[gText.indices[16]]);
                break;
            case 12:
                TB_DrawText(x, y, (gText.indexed + (gBackwards == 0 ? 0 : 1))[gText.indices[16]]);
                break;
            case 13:
                if (gRace == 3) {
                    TB_DrawText(x, y, (gText.indexed + (gCircuitTracks[gCircuitTrack][2] == 0 ? 0 : 1))[gText.indices[16]]);
                } else if (gOptions.mode == 2) {
                    TB_DrawText(x, y, gText.indexed[gText.indices[18] + 2]);
                } else {
                    TB_DrawText(x, y, (gText.indexed + (gOptions.mode == 0 ? 0 : 1))[gText.indices[16]]);
                }
                break;
            case 17:
                sprintf(text, trackscr_drones_format, gNumDrones);
                TB_DrawText(x, y, text);
                break;
            case 20:
                TB_DrawText(x, y, (gText.indexed + (gDeathMode == 0 ? 0 : 1))[gText.indices[16]]);
                break;
            }
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(-1.0f);
}


