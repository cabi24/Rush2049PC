/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ShowPakError group TU (superseding): DrawPakErrNQA (0x800DC88C),
 * DrawChoice3 (0x800DC99C), ControlPakErrNQA + ControlChoice (LOCKED bodies, verbatim m2c text),
 * DrawChoice (0x800DCDF4), DrawPakErrNoPak (0x800DD0C0) and ShowPakError (0x800DD4AC) -- the N64 pak
 * error dialog: ShowPakError finds the active cell of a row, draws the header and dispatches on the cell type.
 * STATE: unit EQUAL for all seven (the whole-program unit, locked bodies differing 0);
 * levers (ShowPakError, from 636/638 rows 277):
 *  - GC-style per-dialog draw/control helpers inlined into ShowPakError (cs_draw1/3, cs_drawT0..3, cs_wait11..13,
 *    cs_ctl10): each owns `u8 str[256]`, which reproduces retail's buffer offsets and its 24 B of inline areas; the
 *    header = two font sets with return copies (fsS_DC single-level, fsL_DC with the nested gfx lock) + an inlined
 *    GC TB_CenterText (TB_CenterText_DC; s16 return copy `move a0,t8`); pair_set_DC writes gJust directly (an
 * inlined `Pair *p` local is one inline-area slot too many, as animPlace).
 *  - DrawChoice/text3 parameter order (row, col, text, a, b, sel) / (row, col, text, s1, s2, s3, sel1, sel2):
 *    ugen evaluates IPA register arguments in prototype order, and retail loads sel/sel1 last; the callee bodies are
 *    unchanged and stay byte-identical (their IPA colours do not depend on the order).
 *  - (strings + lang[n])[1] for the second of a strings pair (retail CSEs the row address), `C.x + C.w / 2` /
 *    `C.y + (C.h - 20) / 2 + 20` operand order (temp ring), the timeout test written on one line (as1 delay slot).
 *  - Source-line layout is load-bearing (as1 ties on line numbers): fsS_DC/fsL_DC keep `old = ...; osSendMesg(...)`
 *    on one line; TB_CenterText_DC and fsL_DC are defined after ShowPakError.
 * DEVICE (disclosed): one compiled-out `if (row) {}` in ShowPakError' column-search loop (had three).
 * No unused locals. Earlier levers: TB_SetJust pair takes s32; text2/text3 separate fh/x locals declared
 * c, y, fh, x; DrawPakErrNoPak indexes the cell directly, declarations x, y, fh, buf, big.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef struct { s32 first, second; } Pair;
typedef struct {
    s8 done;            /* 0 */
    s8 sel1;            /* 1 */
    s8 sel2;            /* 2 */
    u8 pad3;
    s8 state[4];        /* 4 */
    s8 mode[4];         /* 8 */
    u8 pad0C[4];
    s8 alt;             /* 0x10 */
    u8 pad11[3];
    s32 type;           /* 0x14 */
    s16 x, y, w, h;     /* 0x18..0x1E */
    s32 buttons;        /* 0x20 */
    s32 held;           /* 0x24 */
    u32 timeout;        /* 0x28 */
} Cell;                 /* 44 bytes */
typedef struct {
    u8 pad0[4];
    char **msg;         /* +4 */
    u8 pad8[4];
    u16 *lang;          /* +12 */
    char **strings;     /* +16 */
} StrTable;
extern OSMesgQueue text_lock_queue;
extern Pair gJust;
extern Cell gPakError[][2];
extern StrTable gText;
extern s32 gGameState;
extern s32 gActiveEdges;
extern s32 gActiveLevels;
typedef struct Sched { u8 pad0[0x27C]; volatile u32 frameCount; } Sched; /* gScheduler; SetPakError.c */
extern Sched gScheduler;
extern char pak_error_title_format[];
extern char D_80121070[];
extern char D_80121074[];
extern char D_80121078[];
extern char D_8012107C[];
extern char D_80121080[];
extern char D_80121088[];
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern s32 sprintf(char *, const char *, ...);
extern s32 HAL_SetFontInternal(s32 selection);
extern void TB_SetTextColor(s32);
extern s32 TB_GetFontLineHeight(void);
extern u32 HAL_GetTextWidth(void *, s32);
extern void TB_DrawText(s16, s16, char *);
extern void TB_DrawTextScrollBox(s16 arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, s16 arg5, u8 *arg6);
extern u8 *TB_Strcpy(u8 *arg0, void *arg1);
extern u8 *TB_Strcat(u8 *arg0, u8 *arg1);
extern void PlayLRSound(s32 arg0);
extern void PlayStartABSound(s32 arg0);
extern void CancelPakError(s32 arg0);
extern void PlayUDSound(s32 arg0);
extern s32 AudPlayEffect(s32 arg0, s32 arg1, s32 arg2, u8 arg3);
extern void SetControllerActive(s32, s32);
extern void SetControllerStickEdges(s32, s32);
extern void UpdateRushControls(void);

void gfx_lock_DC(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_DC(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_DC(s32 font)
{
    s32 old;

    gfx_lock_DC();
    old = HAL_SetFontInternal(font);
    gfx_unlock_DC();
    return old;
}

s32 font_set1_DC(s32 font)
{
    s32 old;

    osRecvMesg(&text_lock_queue, (void *)0, 1);
    old = HAL_SetFontInternal(font); osSendMesg(&text_lock_queue, (void *)0, 0);
    return old;
}

void pair_set_DC(s32 first, s32 second)
{
    gJust.first = first;
    gJust.second = second;
}

void DrawPakErrNQA(s32 row, s32 col, u8 *text)
{
    Cell *c;

    font_set1_DC(11);
    TB_SetTextColor(1);
    c = &gPakError[row][col];
    pair_set_DC(1, 1);
    TB_DrawTextScrollBox(c->w / 2 + c->x, c->h / 2 + c->y, c->w, c->h, -1, 0, text);
    pair_set_DC(0, 3);
}

void DrawChoice3(s32 row, s32 col, u8 *text, char *s1, char *s2, char *s3, s32 sel1, s32 sel2)
{
    Cell *c;
    s32 y;
    s32 fh;
    s32 x;

    pair_set_DC(1, 1);
    font_set_DC(11);
    TB_SetTextColor(1);
    c = &gPakError[row][col];
    TB_DrawTextScrollBox(c->w / 2 + c->x, (c->h - 40) / 2 + c->y + 20, c->w, c->h - 40, -1, 0, text);
    fh = TB_GetFontLineHeight();
    x = c->w / 2 + c->x;
    y = c->h + c->y - fh * 3;
    font_set_DC(sel1 ? 10 : 11);
    TB_SetTextColor(sel1 ? 22 : 1);
    TB_DrawText(x, y - (sel1 ? 1 : 0), s1);
    font_set_DC(sel2 ? 10 : 11);
    TB_SetTextColor(sel2 ? 22 : 1);
    TB_DrawText(x, y - (sel2 ? 1 : 0) + fh, s2);
    font_set_DC((sel1 || sel2) ? 11 : 10);
    TB_SetTextColor((sel1 || sel2) ? 1 : 22);
    TB_DrawText(x, y - ((sel1 || sel2) ? 0 : 1) + fh * 2, s3);
    pair_set_DC(0, 3);
}

void ControlPakErrNQA(s32 arg1, s32 arg2)
{
  u8 *p;
  s32 v;
  p = ((u8 *) &gPakError) + arg1 * 0x58 + arg2 * 0x2C;
  v = *((s32 *) (p + 0x20));
  if ((v & 7) != 0)
  {
    PlayStartABSound(v);
    *p = 1;
  }
}

void ControlChoice(s32 arg0, s32 arg1)
{
  s32 temp_s0;
  void *temp_s1;
  temp_s1 = (s32 *) (((arg0 * 0x58) + (arg1 * 0x2C)) + ((u8 *) (&gPakError)));
  temp_s0 = *((s32 *) (((s8 *) temp_s1) + 0x20));
  if (temp_s0 & 0xC00)
  {
    PlayLRSound(*((s32 *) (((s8 *) temp_s1) + 0x20)));
    *((s8 *) (((s8 *) temp_s1) + 1)) = (s8)!(*((s8 *) (((s8 *) temp_s1) + 1)));
  }
  if ((*((s32 *) (((s8 *) temp_s1) + 0x20))) & 2)
  {
    AudPlayEffect(0x25, 0, 1, 0U);
    *((s8 *) (((s8 *) temp_s1) + 0)) = 1;
  }
}

void DrawChoice(s32 row, s32 col, intptr_t text, char *a, char *b, s32 sel)
{
    Cell *c;
    s32 y;
    s32 fh;
    s32 x;

    TB_SetTextColor(1);
    font_set_DC(11);
    c = &gPakError[row][col];
    pair_set_DC(1, 1);
    TB_DrawTextScrollBox(c->w / 2 + c->x, (c->h - 40) / 2 + c->y + 20, c->w, c->h - 40, -1, 0, (u8 *)text);
    fh = TB_GetFontLineHeight();
    x = c->w / 2 + c->x;
    y = c->h + c->y - fh * 3;
    font_set_DC(sel ? 11 : 10);
    TB_SetTextColor(sel ? 1 : 22);
    TB_DrawText(x, y - (sel ? 0 : 1) + fh, gText.msg[944 / 4]);
    font_set_DC(sel ? 10 : 11);
    TB_SetTextColor(sel ? 22 : 1);
    TB_DrawText(x, y - (sel ? 1 : 0), gText.strings[gText.lang[25]]);
    pair_set_DC(0, 3);
}

void DrawPakErrNoPak(s32 row, s32 col)
{
    s32 x;
    s32 y;
    s32 fh;
    u8 buf[256];
    s32 big;

    big = !(gGameState & 0x7C03FFFE);
    font_set_DC(11);
    TB_SetTextColor(1);
    TB_Strcpy(buf, gText.msg[416 / 4]);
    TB_Strcat(buf, (u8 *)(big ? gText.msg[412 / 4] : gText.msg[408 / 4]));
    pair_set_DC(1, 1);
    TB_DrawTextScrollBox(gPakError[row][col].x + gPakError[row][col].w / 2, gPakError[row][col].y + (gPakError[row][col].h - (big ? 40 : 20)) / 2 + 20, gPakError[row][col].w, gPakError[row][col].h - (big ? 40 : 20),
                       -1, 0, buf);
    if (big) {
        fh = TB_GetFontLineHeight();
        x = gPakError[row][col].x + gPakError[row][col].w / 2;
        y = gPakError[row][col].y + gPakError[row][col].h - fh * 3;
        font_set_DC(gPakError[row][col].sel1 ? 11 : 10);
        TB_SetTextColor(gPakError[row][col].sel1 ? 1 : 22);
        TB_DrawText(x, y - (gPakError[row][col].sel1 ? 0 : 1) + fh, gText.msg[944 / 4]);
        font_set_DC(gPakError[row][col].sel1 ? 10 : 11);
        TB_SetTextColor(gPakError[row][col].sel1 ? 22 : 1);
        TB_DrawText(x, y - (gPakError[row][col].sel1 ? 1 : 0), gText.strings[gText.lang[25]]);
    }
    pair_set_DC(0, 3);
    font_set_DC(11);
}

s32 fsS_DC(s32 font)
{
    s32 old;

    osRecvMesg(&text_lock_queue, (void *)0, 1);
    old = HAL_SetFontInternal(font); osSendMesg(&text_lock_queue, (void *)0, 0);
    return old;
}

s16 TB_CenterText_DC(char *text, s16 centerX);
s32 fsL_DC(s32 font);
#define C (gPakError[row][col])
void cs_draw1_DC(s32 row, s32 col)
{
    u8 str[256];

    font_set_DC(11);
    TB_SetTextColor(1);
    TB_Strcpy(str, gText.msg[404 / 4]);
    TB_Strcat(str, (u8 *)D_80121070);
    TB_Strcat(str, (u8 *)gText.msg[408 / 4]);
    pair_set_DC(1, 1);
    TB_DrawTextScrollBox(C.x + C.w / 2, C.y + (C.h - 20) / 2 + 20, C.w, C.h - 20, -1, 0, str);
    pair_set_DC(0, 3);
    font_set_DC(11);
}

void cs_draw3_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, C.alt ? gText.msg[424 / 4] : gText.msg[428 / 4]);
    TB_Strcat(str, (u8 *)D_80121074);
    TB_Strcat(str, (u8 *)gText.msg[432 / 4]);
    DrawChoice(row, col, (intptr_t)str, gText.strings[gText.lang[25]], gText.msg[944 / 4], C.sel1);
}

void cs_drawT0_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[436 / 4]);
    TB_Strcat(str, (u8 *)D_80121078);
    TB_Strcat(str, (u8 *)gText.msg[440 / 4]);
    DrawPakErrNQA(row, col, str);
}

void cs_drawT1_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[444 / 4]);
    TB_Strcat(str, (u8 *)D_80121088);
    TB_Strcat(str, (u8 *)gText.msg[408 / 4]);
    DrawPakErrNQA(row, col, str);
}

void cs_drawT2_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[452 / 4]);
    TB_Strcat(str, (u8 *)D_8012107C);
    TB_Strcat(str, (u8 *)gText.msg[456 / 4]);
    DrawPakErrNQA(row, col, str);
}

void cs_drawT3_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[460 / 4]);
    TB_Strcat(str, (u8 *)D_80121080);
    TB_Strcat(str, (u8 *)gText.msg[440 / 4]);
    DrawPakErrNQA(row, col, str);
}

void cs_wait11_DC(s32 row, s32 col)
{
    if (C.timeout < gScheduler.frameCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.msg[476 / 4]);
}

void cs_wait12_DC(s32 row, s32 col)
{
    if (C.timeout < gScheduler.frameCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.msg[480 / 4]);
}

void cs_wait13_DC(s32 row, s32 col)
{
    if (C.timeout < gScheduler.frameCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.msg[484 / 4]);
}

void cs_ctl10_DC(Cell *c)
{
    if (c->buttons & 0x800) {
        PlayUDSound(c->buttons);
        if (c->sel1) {
            c->sel1 = 0;
            c->sel2 = 1;
        } else if (c->sel2) {
            c->sel2 = 0;
        } else {
            c->sel1 = 1;
        }
    }
    if (c->buttons & 0x400) {
        PlayUDSound(c->buttons);
        if (c->sel1) {
            c->sel1 = 0;
        } else if (c->sel2) {
            c->sel2 = 0;
            c->sel1 = 1;
        } else {
            c->sel2 = 1;
        }
    }
    if (c->buttons & 2) {
        AudPlayEffect(37, 0, 1, 0);
        c->done = 1;
    }
}

void ShowPakError(s32 row, s8 *sel1, s8 *sel2, s8 *done, s32 (*poll)(void))
{
    s32 col;
    s32 i;
    char num[72];

    if (poll != 0 && poll() != 0) {
        CancelPakError(row);
    }
    for (col = 1; col >= 0; col--) {
        if (C.type != 0) {
            break;
        }
        if (row) {
        }
        if (C.done == 0) {
            C.done = 1;
            *done = 1;
            *sel2 = 0;
            *sel1 = 0;
            goto apply;
        }
    }
    if (col < 0) {
        return;
    }
    C.buttons = gActiveEdges;
    C.held = gActiveLevels;
    fsS_DC(11);
    TB_SetTextColor(1);
    fsL_DC(11);
    TB_SetTextColor(1);
    sprintf(num, pak_error_title_format, row + 1);
    TB_DrawText(TB_CenterText_DC(num, C.x + C.w / 2), C.y, num);
    switch (C.type) {
    case 1:
        ControlChoice(row, col);
        cs_draw1_DC(row, col);
        break;
    case 2:
        if (!(gGameState & 0x7C03FFFE)) {
            ControlChoice(row, col);
        } else {
            ControlPakErrNQA(row, col);
        }
        DrawPakErrNoPak(row, col);
        break;
    case 3:
        ControlChoice(row, col);
        cs_draw3_DC(row, col);
        break;
    case 4:
        ControlPakErrNQA(row, col);
        cs_drawT0_DC(row, col);
        break;
    case 15:
        ControlPakErrNQA(row, col);
        cs_drawT1_DC(row, col);
        break;
    case 5:
    case 7:
        ControlChoice(row, col);
        DrawChoice(row, col, (intptr_t)gText.msg[444 / 4], (gText.strings + gText.lang[26])[1], gText.msg[944 / 4], C.sel1);
        break;
    case 8:
        ControlChoice(row, col);
        DrawChoice(row, col, (intptr_t)gText.msg[448 / 4], gText.strings[gText.lang[24]], (gText.strings + gText.lang[24])[1], C.sel1);
        break;
    case 6:
        ControlChoice(row, col);
        cs_drawT2_DC(row, col);
        break;
    case 9:
        ControlPakErrNQA(row, col);
        cs_drawT3_DC(row, col);
        break;
    case 10:
        cs_ctl10_DC(&C);
        DrawChoice3(row, col, (u8 *)gText.msg[472 / 4], gText.msg[944 / 4], gText.strings[gText.lang[26]], (gText.strings + gText.lang[26])[1], C.sel1, C.sel2);
        break;
    case 11:
        cs_wait11_DC(row, col);
        break;
    case 12:
        cs_wait12_DC(row, col);
        break;
    case 13:
        cs_wait13_DC(row, col);
        break;
    case 14:
        C.done = 1;
        break;
    }
    *done = C.done;
    if (C.done) {
        C.type = 0;
        *sel1 = C.sel1;
        *sel2 = C.sel2;
apply:
        for (i = 0; i < 4; i++) {
            SetControllerActive(i, C.state[i]);
            SetControllerStickEdges(i, C.mode[i]);
        }
        UpdateRushControls();
    }
}

s16 TB_CenterText_DC(char *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
}

s32 fsL_DC(s32 font)
{
    s32 old;

    gfx_lock_DC();
    old = HAL_SetFontInternal(font); osSendMesg(&text_lock_queue, (void *)0, 0);
    return old;
}
