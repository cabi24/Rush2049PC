/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Controls screen text, controlscr.c (MAT3/Dreamcast controlscr.c).
 *   controls_animText (0x80100564, kept: blit anim callback) = animText@controlscr.c
 *   controls_animButtonText (0x800FFDE8, internal, only caller controls_animText; p passed in the caller's
 *     outgoing slot 0 by the private IPA convention) = animButtonText@controlscr.c(signed char p)
 * N64 differences from MAT3/DC: 12 controls (MAT3 13), rumble row i == 10 (MAT3 11), centred row i == 11,
 * no /2 on projected coordinates, `x - 35/55/45/25 * scale * 300 / buttonZ` operand order, the N64 validOption
 * (controls_validOption, rejects option 8) and PS_PakRumble (PS_PakRumble) inlined.
 * Inlined helpers, non-static and not on keep (stub rule), disclosed:
 *   ct_gfx_lock / ct_gfx_unlock / ct_font_set = the two-level HAL_SetFont (_GetSemaphore; old = _HAL_SetFont;
 *   _ReleaseSemaphore), ct_SetJust = TB_SetJust, ct_CenterText / ct_RightText = TB_CenterText / TB_RightText,
 * ct_DrawEllipseText = TB_DrawEllipseText (defined after its caller: as1 line ties).
 * levers (each measured in the unit):
 *   animButtonText: the ellipse width as its own statement `w = gNumWindows == 1 ? 150 : 69;` (its line precedes
 *     the name load, as1 then orders `li v0,69` before the gPlayers reload, as retail) and the name in `str`
 *     (str and the inlined `text` parameter share a3, retail `lw a3; addiu a1,a3,20` / `move a2,a3`).
 *   animText: `continue` after the animButtonText call (retail's call path carries its own p+1/gNumWindows
 *     copy into the loop test: body 390 -> 394 words); TB_SetJust as the helper at the scroll-box site and
 *     HAL_SetFont called directly (frame 352 -> 360, every spill home at retail's offset); itemsDrawn = 0
 *     first in the menu branch (as1 line tie on `move s5,zero`).
 * DISCLOSED shaping:
 *   - animText `if (i == 10) {}` (compiled out, emits nothing): one more use of the constant 10 so its
 *     loop-wide web (retail s8, two `bne s2,s8`) wins the 11.11-save tie against the PS_PakRumble address and
 *     stride webs (lower web numbers); without it s8 goes to p*2 (unit: 362 of 394 words differ).
 *   - animButtonText `u8 str1[64]` is UNUSED: exact frame residual (retail frame 544 with screenPos at sp+456
 *     and the incoming p byte at sp+547; without it the frame is 480 and p is read at sp+483: unit 7 of 479
 *     words differ). One 64-byte slot, declared before screenPos; animText declares the same `str1[64]` text
 *     buffer. MAT3/GC DWARF list no such local (i, x, y, str, screenPos, gTextPosScale). `w` (used) is
 *     declared after screenPos.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct CtIdx { u8 pad0[32]; u16 rumbleOff; u16 rumbleNames; u8 pad36[2]; u16 controlNames; } CtIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; CtIdx *indices; char **strings; } TextState;
typedef struct PlayerRow { u8 vehicle; u8 pak; u8 other[70]; char **name; } PlayerRow;   /* 76 B */
/* *name is the player's profile record (save.c's SaveSlot): 64 bytes on the PC, its name at +0x20 and the
 * rumble byte at +0x37 (PROFILE_OFS, pc_prelude.h; docs/port/POINTERS.md 9.10). */
typedef struct CtRect { s32 f0; s32 y; s32 w; s32 h; s32 x; s32 f20; } CtRect;          /* 24 B */
typedef struct PakSlot { s8 b0, b1, b2, b3, b4, b5, b6; u8 rest[765]; } PakSlot;          /* 772 B */
typedef struct ContData { s8 b0; u8 rest[15]; } ContData;                                   /* 16 B */

extern OSMesgQueue text_lock_queue;
extern TextState gText;                /* gText */
extern s16 gNumWindows;                           /* gNumWindows */
extern s32 controls_buttonsInited[];                         /* buttonsInited[p] */
extern PlayerRow gPlayers[];                   /* gPlayers */
extern s8 controlScrActive[];                          /* controlScrActive[p] */
extern CtRect controls_gPlayerRects[][4];                   /* gPlayerRects (24-byte form) */
extern ContData gControllerData[];                    /* gControllerData */
extern s32 playerTextColor[];                         /* playerTextColor */
extern s32 gGameState;                         /* gGameState */
extern s16 gControlFirst[];                         /* gControlFirst[p] */
extern s16 gControlSelect[];                         /* gControlSelect[p] */
extern s16 gControlMenuSize;                           /* gControlMenuSize */
extern PakSlot gPaks[];
extern s32 gJust[2];   /* gJust (TB_SetJust pair): tentative definition, as hud_text_anims/audio_animPanes_demo_animText.c, countdown,
                        coins: the two TB_SetJust stores share one `lui at` only with gJust defined in the group's
                        program; in the shadow unit audio_animPanes_demo_animText.c is the one definition (unit_overrides extern_data) */
extern char controller_removed_format[];                        /* "CONTROLLER %d %s" */
extern s32 osRecvMesg(OSMesgQueue *, void **, s32), osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32);
extern u32 HAL_GetTextWidth(char *, s32);   /* HAL_GetTextWidth */
extern s16 TB_GetFontLineHeight(void);        /* TB_GetFontLineHeight */
extern void TB_SetTextColor(s32);               /* TB_SetTextColor */
extern void TB_DrawText(s16, s16, char *);     /* TB_DrawText */
extern s32 TB_Sprintf(char *, char *, ...);    /* TB_Sprintf */
extern void HAL_SetTextDepth(f32);                  /* HAL_SetTextDepth */
extern void TB_EllipseText(char *, char *, s16);   /* TB_EllipseText (retail narrows the width: s16) */
extern void TB_DrawTextScrollBox(s16, s16, s16, s16, s32, s32, char *);  /* TB_DrawTextScrollBox */
typedef struct CtButton { u8 pad[8]; f32 angle; u8 pad12[36]; u8 pos[8]; f32 depth; u8 alpha; u8 pad61[3]; } CtButton; /* 64 B */
typedef struct CtCamera { u8 data[152]; } CtCamera;
extern CtButton controls_buttonInfo[][17];                /* buttonInfo[p][17] */
extern CtCamera gCamera[];                    /* gCamera[p] */
extern f32 controls_gButtonScale;                           /* gButtonScale */
extern f32 controls_buttonZ;                           /* buttonZ */
extern void ZOID_ProjectWindow(s32, void *, void *, void *, s16 *);  /* ZOID_ProjectWindow */
extern void HAL_SetTextTranslucency(f32);                  /* HAL_SetTextTranslucency */
extern void HAL_SetMenuTextDepth(f32);                  /* HAL_SetMenuTextDepth */
typedef union TextColor { u8 c[4]; u32 rgba; } TextColor;   /* GC union __anon_21da */
extern TextColor flashColors[2];                  /* flashColors */
extern TextColor gFlashColor1, gFlashColor2;
void TB_FlashTextColors(TextColor first, TextColor second);   /* TB_FlashTextColors (locked, audioscr) */
void controls_animButtonText(s8 p);
void TB_DrawEllipseText(s16 x, s16 y, s16 width, void *name);   /* TB_DrawEllipseText (locked, kept) */

void ct_gfx_lock(void) { osRecvMesg(&text_lock_queue, 0, 1); }
void ct_gfx_unlock(void) { osSendMesg(&text_lock_queue, 0, 0); }
s32 ct_font_set(s32 font)
{
    s32 old;
    ct_gfx_lock();
    old = HAL_SetFontInternal(font);
    ct_gfx_unlock();
    return old;
}
s16 ct_CenterText(char *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
}
s16 ct_RightText(char *text, s16 rightX)
{
    return rightX - HAL_GetTextWidth(text, -1);
}
s8 PS_PakRumble(s32 pak);                       /* PS_PakRumble (locked, SaveInit) */
s32 controls_validOption(s8 player, s32 item);          /* validOption (group member) */

void ct_DrawEllipseText(s16 x, s16 y, s16 w, void *text);
void ct_SetJust(s32 h, s32 v)
{
    gJust[0] = h;
    gJust[1] = v;
}
void controls_animButtonText(s8 p)
{
    s32 i;
    s32 x;
    s32 y;
    char *str;
    u8 str1[64];
    f32 gTextPosScale;
    s16 screenPos[2];
    s32 w;

    gTextPosScale = controls_gButtonScale;
    if (gNumWindows > 1) gTextPosScale *= 0.5f;
    ct_font_set(11);
    ct_SetJust(1, 1);
    if (gPlayers[p].pak != 5 && (controlScrActive[p] == 1 || (gGameState & 0x7C0000))) {
        ZOID_ProjectWindow(p, controls_buttonInfo[p][13].pos, &gCamera[p], 0, screenPos);
        x = screenPos[0];
        y = screenPos[1];
        if (gNumWindows > 1) ct_font_set(11);
        else ct_font_set(13);
        TB_SetTextColor(1);
        w = gNumWindows == 1 ? 150 : 69;
        str = *gPlayers[p].name + PROFILE_OFS(20);
        ct_DrawEllipseText(x, y, w, str);
        ct_SetJust(0, 1);
        if (!controlScrActive[p]) {
            if (gGameState & 0x7C0000) {
                if (gNumWindows == 1) ct_font_set(10);
                else ct_font_set(11);
                TB_SetTextColor(1);
                ct_SetJust(1, 1);
                TB_DrawTextScrollBox(controls_gPlayerRects[gNumWindows - 1][p].x, controls_gPlayerRects[gNumWindows - 1][p].f20,
                                   controls_gPlayerRects[gNumWindows - 1][p].w, controls_gPlayerRects[gNumWindows - 1][p].h,
                                   -1, 0, gText.fixed[38]);
                ct_SetJust(0, 3);
            }
        } else {
            if (gNumWindows == 1) ct_font_set(11);
            else ct_font_set(12);
            for (i = 0; i < 12; i++) {
                if (controls_buttonInfo[p][i].angle > 0.69813168f && controls_buttonInfo[p][i].angle < 2.443461f) continue;
                if (!controls_buttonInfo[p][i].alpha) continue;
                HAL_SetTextTranslucency(controls_buttonInfo[p][i].alpha);
                HAL_SetMenuTextDepth(controls_buttonInfo[p][i].depth - 10.0f);
                ZOID_ProjectWindow(p, controls_buttonInfo[p][i].pos, &gCamera[p], 0, screenPos);
                if (i == 10) x = screenPos[0] - 35.0f * gTextPosScale * 300.0f / controls_buttonZ;
                else if (i == 11) x = screenPos[0] - 55.0f * gTextPosScale * 300.0f / controls_buttonZ;
                else x = screenPos[0] - 45.0f * gTextPosScale * 300.0f / controls_buttonZ;
                y = screenPos[1];
                if (controls_buttonInfo[p][i].angle < 1.5707964f) {
                    TB_FlashTextColors(flashColors[0], flashColors[1]);
                    TB_SetTextColor(1);
                    if (!PS_PakRumble(gPlayers[p].pak) && i == 10) TB_SetTextColor(3);
                } else {
                    TB_FlashTextColors(flashColors[0], flashColors[1]);
                    TB_SetTextColor(22);
                }
                str = gText.strings[gText.indices->controlNames + i];
                TB_DrawText(x, y, str);
                if (i == 10) {
                    ZOID_ProjectWindow(p, controls_buttonInfo[p][12].pos, &gCamera[p], 0, screenPos);
                    x = screenPos[0] - 25.0f * gTextPosScale * 300.0f / controls_buttonZ;
                    y = screenPos[1];
                    if (!PS_PakRumble(gPlayers[p].pak)) str = gText.strings[gText.indices->rumbleOff];
                    else str = gText.strings[gText.indices->rumbleNames + ((s8 *)*gPlayers[p].name)[PROFILE_OFS(43)]];
                    TB_DrawText(x, y, str);
                }
            }
        }
    }
    ct_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(0.0f);
}

void ct_DrawEllipseText(s16 x, s16 y, s16 w, void *text)
{
    u8 str[128];
    TB_EllipseText((char *)str, text, w);
    TB_DrawText(x, y, (char *)str);
}
s32 controls_animText(s32 arg0)
{
    s32 i;
    s32 p;
    s32 x;
    s32 y;
    char *str;
    char str1[64];
    char pName[14];
    s32 itemsDrawn;

    HAL_SetTextDepth(0.0f);
    for (p = 0; p < gNumWindows; p++) {
        if (controls_buttonsInited[p] == 1) {
            controls_animButtonText(p);
            continue;
        }
        ct_font_set(11);
        if (gPlayers[p].pak != 5 && (controlScrActive[p] == 1 || (gGameState & 0x7C0000))) {
            y = controls_gPlayerRects[gNumWindows - 1][p].y + 10;
            x = controls_gPlayerRects[gNumWindows - 1][p].x;
            TB_SetTextColor(playerTextColor[p]);
            if (!gControllerData[p].b0) {
                TB_Sprintf(str1, controller_removed_format, p + 1, gText.fixed[233]);
            } else {
                TB_EllipseText(pName, *gPlayers[p].name + PROFILE_OFS(20), 69);
                TB_Sprintf(str1, gText.fixed[37], pName);
            }
            TB_DrawText(ct_CenterText(str1, x), y, str1);
            TB_GetFontLineHeight();
            if (!controlScrActive[p]) {
                if (gGameState & 0x7C0000) {
                    TB_SetTextColor(1);
                    ct_SetJust(1, 1);
                    TB_DrawTextScrollBox(controls_gPlayerRects[gNumWindows - 1][p].x, controls_gPlayerRects[gNumWindows - 1][p].f20,
                                       controls_gPlayerRects[gNumWindows - 1][p].w / 2, controls_gPlayerRects[gNumWindows - 1][p].h,
                                       -1, 0, gText.fixed[38]);
                    ct_SetJust(0, 3);
                }
            } else {
                itemsDrawn = 0;
                x = controls_gPlayerRects[gNumWindows - 1][p].x + 10;
                y = controls_gPlayerRects[gNumWindows - 1][p].y + 24;
                for (i = 0; i < 12; i++) {
                    if (i >= gControlFirst[p] && controls_validOption(p, i)) {
                        if (i == 10) {}
                        if (!PS_PakRumble(gPlayers[p].pak) && i == 10)
                            TB_SetTextColor(3);
                        else
                            TB_SetTextColor(i == gControlSelect[p] ? 22 : 1);
                        str = gText.strings[gText.indices->controlNames + i];
                        if (i == 11)
                            TB_DrawText(ct_CenterText(str, controls_gPlayerRects[gNumWindows - 1][p].x), y, str);
                        else
                            TB_DrawText(ct_RightText(str, x), y, str);
                        if (i == 10) {
                            if (!PS_PakRumble(gPlayers[p].pak))
                                str = gText.strings[gText.indices->rumbleOff];
                            else
                                str = gText.strings[gText.indices->rumbleNames + ((s8 *)*gPlayers[p].name)[PROFILE_OFS(43)]];
                            TB_DrawText(x + 20, y, str);
                        }
                        y += 16;
                        if (++itemsDrawn >= gControlMenuSize) break;
                    }
                }
            }
        }
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
