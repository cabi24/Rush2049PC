/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animSpeedText (0x800EF8F4, 1460 B) = MAT3 animSpeedText@hud.c (GC 0x8001DB6C, 864 B): the digital
 * speedometer text. Direct typed port of the MAT3 body.
 * : the three HAL_SetFont calls go to the REAL HAL_SetFont (HAL_SetFont, two-level form installed in state,
 * umerge-inlined in the unit) instead of the private HAL_SetFont_Speed copy: body length now exact (365 words) and
 * `FAIL animSpeedText: 31 of 365 words differ` (x2: 282 of 365, 366 words). NOT a match.
 * Kept from x2: `if (y) {}` (compiled-out holder), private TB_SetJust/TB_GetFontHeight copies (_Speed).
 * Residual: split pieces &gGameCar / 952 / 1 / 10 are a2 / v0 / t0 / a3 (retail v0 / a2 / a3 / a2).
 * PC (compiled into the PC build, CMakeLists.txt): no direct callers and no code takes its address, so it is reached
 * through an entry of the HUD's MultiBlit tables, which ShowHUD (ShowHUD, showhud.c) hands to NewMultiBlit
 * (the tables are ROM data, so which entry is not visible here). NewMultiBlit (NewMultiBlit.c) makes an entry's
 * function the sprite's draw callback (a -1 name; the 2D sprite pass of ZOID_EndFrame calls it every
 * frame) or the blit's AnimFunc; either way it gets the blit. The car record has its PC layout (docs/port/CARS.md);
 * the text bank's string table is the host pointer at +8 (ModelTables).
 */
#define model D_8014A250
#define gFontSem text_lock_queue
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: the car record's layout (0x468 bytes, docs/port/CARS.md). */
typedef struct tCarData {
    u8 pad000[0xEF]; s8 place_locked; u8 pad0F0[8]; s16 rwmph; u8 pad0FA[0x110 - 0xFA]; struct pc_visual visuals[21];
    u8 pad0FA_308[0x380 - 0x308]; void *input; u8 pad0FA_384[952 - 0x384];
} tCarData;
typedef struct tModelData { u8 pad000[0x7C6]; s16 slot; u8 pad7C8[0x808 - 0x7C8]; } tModelData;
typedef struct tJust { s32 h, v; } tJust;
typedef struct tText { u16 numSingle; u8 **single; } tText;
typedef struct tFontHeader { u8 pad0[2]; u8 ascent; u8 descent; } tFontHeader;

extern s8 gShowSpeed;
extern tJust gJust;
extern s16 gNumWindows;
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gShowMetric;
extern s32 speedPos[4][4][2];        /* speedPos */
extern tText gText;
extern tFontHeader *gFontHeader;
extern OSMesgQueue gFontSem;         /* text_lock_queue */

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
void HAL_SetTextDepth(f32 depth);
s32 HAL_SetFontInternal(s32 fontID);              /* _HAL_SetFont */
void SetupFontData(s32 force);          /* _SetupFontData */
u32 HAL_GetCharWidth(u16 ch);
void TB_SetTextColor(s32 color);
void TB_DrawText(s16 x, s16 y, void *text);

void _GetSemaphore_Speed(void);
void _ReleaseSemaphore_Speed(void);
s32 HAL_SetFont(s32 fontID);
void TB_SetJust_Speed(s32 hJust, s32 vJust);
u8 HAL_GetFontAscent_Speed(void);
u8 HAL_GetFontDescent_Speed(void);
s16 TB_GetFontHeight_Speed(void);
s16 TB_GetFontHeight(void);

s32 animSpeedText(void *blt)
{
    f32 mph;
    u8 digits[3];
    u8 str[2];
    s32 digitW, x, y, digitIdx, slot;

    if (!gShowSpeed)
        return 1;
    HAL_SetTextDepth(0.0f);
    TB_SetJust_Speed(1, 3);
    if (gNumWindows == 1)
        HAL_SetFont(4);
    else
        HAL_SetFont(1);
    digitW = HAL_GetCharWidth('8') + 1;
    for (slot = 0; slot < gNumWindows; slot++) {
        if (y) {}
        if (PC_CAR_FINISHED(PC_MODEL_SLOT(slot)) == 1)
            continue;
        mph = PC_CAR_MPH(slot) >> 2;
        mph *= 1.2f;
        if (gShowMetric)
            mph *= 1.61f;
        if (mph < 0.0f)
            mph = -mph;
        x = speedPos[gNumWindows - 1][slot][0];
        x -= digitW;
        y = speedPos[gNumWindows - 1][slot][1];
        digits[0] = ((s32)(mph / 100.0f) % 10) + '0';
        digits[1] = ((s32)(mph / 10.0f) % 10) + '0';
        digits[2] = ((s32)mph % 10) + '0';
        if (gNumWindows == 1)
            HAL_SetFont(4);
        else
            HAL_SetFont(1);
        for (digitIdx = 0; digitIdx < 3; digitIdx++) {
            str[1] = 0;
            if (digits[digitIdx] != ':' && digits[digitIdx] != '.') {
                str[0] = '8';
                TB_SetTextColor(0);
                TB_DrawText(x, y, str);
            }
            str[0] = digits[digitIdx];
            TB_SetTextColor(14);
            TB_DrawText(x, y, str);
            x += digitW;
        }
        x = speedPos[gNumWindows - 1][slot][0];
        y += TB_GetFontHeight() - 4;
        HAL_SetFont(12);
        if (gShowMetric) {
            TB_SetTextColor(0);
            TB_DrawText(x + 1, y + 1, gText.single[203]);
            TB_SetTextColor(1);
            TB_DrawText(x, y, gText.single[203]);
        } else {
            TB_SetTextColor(0);
            TB_DrawText(x + 1, y + 1, gText.single[202]);
            TB_SetTextColor(1);
            TB_DrawText(x, y, gText.single[202]);
        }
    }
    TB_SetJust_Speed(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

void TB_SetJust_Speed(s32 hJust, s32 vJust)
{
    gJust.h = hJust;
    gJust.v = vJust;
}

u8 HAL_GetFontAscent_Speed(void)
{
    SetupFontData(0);
    return gFontHeader->ascent;
}

u8 HAL_GetFontDescent_Speed(void)
{
    SetupFontData(0);
    return gFontHeader->descent;
}

s16 TB_GetFontHeight_Speed(void)
{
    return HAL_GetFontAscent_Speed() + HAL_GetFontDescent_Speed();
}
