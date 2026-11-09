#include "game/game.h"

typedef struct Cell {
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

VIEW(Cell[][2], gPakError);
#define gPakError gPakError_view
VIEW(StrTable, gText);
#define gText gText_view
VIEW(s32, gGameState);
#define gGameState gGameState_view

typedef struct Sched { u8 pad0[0x27C]; volatile u32 frameCount; } Sched; /* gScheduler; SetPakError.c */
VIEW(Sched, gScheduler);
#define gScheduler gScheduler_view

extern s32 sprintf(char *, const char *, ...);

void DrawPakErrNQA(s32 row, s32 col, u8 *text)
{
    Cell *c;

    HAL_SetFont(11);
    TB_SetTextColor(1);
    c = &gPakError[row][col];
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(c->w / 2 + c->x, c->h / 2 + c->y, c->w, c->h, -1, 0, text);
    TB_SetJust(0, 3);
}

void DrawChoice3(s32 row, s32 col, u8 *text, char *s1, char *s2, char *s3, s32 sel1, s32 sel2)
{
    Cell *c;
    s32 y;
    s32 fh;
    s32 x;

    TB_SetJust(1, 1);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    c = &gPakError[row][col];
    TB_DrawTextScrollBox(c->w / 2 + c->x, (c->h - 40) / 2 + c->y + 20, c->w, c->h - 40, -1, 0, text);
    fh = TB_GetFontLineHeight();
    x = c->w / 2 + c->x;
    y = c->h + c->y - fh * 3;
    HAL_SetFont(sel1 ? 10 : 11);
    TB_SetTextColor(sel1 ? 22 : 1);
    TB_DrawText(x, y - (sel1 ? 1 : 0), s1);
    HAL_SetFont(sel2 ? 10 : 11);
    TB_SetTextColor(sel2 ? 22 : 1);
    TB_DrawText(x, y - (sel2 ? 1 : 0) + fh, s2);
    HAL_SetFont((sel1 || sel2) ? 11 : 10);
    TB_SetTextColor((sel1 || sel2) ? 1 : 22);
    TB_DrawText(x, y - ((sel1 || sel2) ? 0 : 1) + fh * 2, s3);
    TB_SetJust(0, 3);
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
    HAL_SetFont(11);
    c = &gPakError[row][col];
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(c->w / 2 + c->x, (c->h - 40) / 2 + c->y + 20, c->w, c->h - 40, -1, 0, (u8 *)text);
    fh = TB_GetFontLineHeight();
    x = c->w / 2 + c->x;
    y = c->h + c->y - fh * 3;
    HAL_SetFont(sel ? 11 : 10);
    TB_SetTextColor(sel ? 1 : 22);
    TB_DrawText(x, y - (sel ? 0 : 1) + fh, gText.msg[944 / 4]);
    HAL_SetFont(sel ? 10 : 11);
    TB_SetTextColor(sel ? 22 : 1);
    TB_DrawText(x, y - (sel ? 1 : 0), gText.strings[gText.lang[25]]);
    TB_SetJust(0, 3);
}

void DrawPakErrNoPak(s32 row, s32 col)
{
    s32 x;
    s32 y;
    s32 fh;
    u8 buf[256];
    s32 big;

    big = !(gGameState & 0x7C03FFFE);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    TB_Strcpy(buf, gText.msg[416 / 4]);
    TB_Strcat(buf, (u8 *)(big ? gText.msg[412 / 4] : gText.msg[408 / 4]));
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(gPakError[row][col].x + gPakError[row][col].w / 2, gPakError[row][col].y + (gPakError[row][col].h - (big ? 40 : 20)) / 2 + 20, gPakError[row][col].w, gPakError[row][col].h - (big ? 40 : 20),
                       -1, 0, buf);
    if (big) {
        fh = TB_GetFontLineHeight();
        x = gPakError[row][col].x + gPakError[row][col].w / 2;
        y = gPakError[row][col].y + gPakError[row][col].h - fh * 3;
        HAL_SetFont(gPakError[row][col].sel1 ? 11 : 10);
        TB_SetTextColor(gPakError[row][col].sel1 ? 1 : 22);
        TB_DrawText(x, y - (gPakError[row][col].sel1 ? 0 : 1) + fh, gText.msg[944 / 4]);
        HAL_SetFont(gPakError[row][col].sel1 ? 10 : 11);
        TB_SetTextColor(gPakError[row][col].sel1 ? 22 : 1);
        TB_DrawText(x, y - (gPakError[row][col].sel1 ? 1 : 0), gText.strings[gText.lang[25]]);
    }
    TB_SetJust(0, 3);
    HAL_SetFont(11);
}


#define C (gPakError[row][col])
void cs_draw1_DC(s32 row, s32 col)
{
    u8 str[256];

    HAL_SetFont(11);
    TB_SetTextColor(1);
    TB_Strcpy(str, gText.msg[404 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_no_controller);
    TB_Strcat(str, (u8 *)gText.msg[408 / 4]);
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(C.x + C.w / 2, C.y + (C.h - 20) / 2 + 20, C.w, C.h - 20, -1, 0, str);
    TB_SetJust(0, 3);
    HAL_SetFont(11);
}

void cs_draw3_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, C.alt ? gText.msg[424 / 4] : gText.msg[428 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_new_pak);
    TB_Strcat(str, (u8 *)gText.msg[432 / 4]);
    DrawChoice(row, col, (intptr_t)str, gText.strings[gText.lang[25]], gText.msg[944 / 4], C.sel1);
}

void cs_drawT0_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[436 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_inconsistent);
    TB_Strcat(str, (u8 *)gText.msg[440 / 4]);
    DrawPakErrNQA(row, col, str);
}

void cs_drawT1_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[444 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_unknown);
    TB_Strcat(str, (u8 *)gText.msg[408 / 4]);
    DrawPakErrNQA(row, col, str);
}

void cs_drawT2_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[452 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_full);
    TB_Strcat(str, (u8 *)gText.msg[456 / 4]);
    DrawPakErrNQA(row, col, str);
}

void cs_drawT3_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[460 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_bad_data);
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
    HAL_SetFont(11);
    TB_SetTextColor(1);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    sprintf(num, pak_error_title_format, row + 1);
    TB_DrawText(TB_CenterText(num, C.x + C.w / 2), C.y, num);
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


