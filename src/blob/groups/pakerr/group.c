/* The Controller Pak error dialogs (MAT3 pakerr.c): a box per controller (gPakError[pak][id], two a pak) that
 * says what is wrong with the pak (none, a new one, damaged, full, bad data ...) and takes the player's answer.
 * ShowPakError draws and runs them each frame; SetPakError (elsewhere) opens one. While a dialog is up the
 * controllers it lists are made active (and their sticks drive the d-pad) as it asks. */
#include "game/game.h"

/* A pak error dialog (MAT3 anon_done_choice1_34, the N64's 44 bytes). */
typedef struct Cell {
    s8 done;            /* 0x00: answered (or timed out) */
    s8 choice1;         /* 0x01: the first answer chosen (of two or three) */
    s8 choice2;         /* 0x02: the second answer chosen (of three) */
    u8 pad3;
    s8 contActive[4];   /* 0x04: which controllers count as active while it is up */
    s8 contStick[4];    /* 0x08: which controllers' sticks drive the d-pad */
    u8 pad0C[4];
    s8 alt;             /* 0x10: which of two messages the new-pak dialog shows */
    u8 pad11[3];
    s32 err;            /* 0x14: the error (the cases of ShowPakError), 0 when no dialog */
    s16 left, top, width, height;   /* 0x18: its box on the screen */
    s32 edges;          /* 0x20: the buttons pressed this frame */
    s32 levels;         /* 0x24: held */
    u32 timeout;        /* 0x28: the retrace count at which a waiting dialog closes itself */
} Cell;                 /* 44 bytes */
/* The game's text (gText): msg[] the messages by number, strings[] with lang[] the words of the language. */
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

typedef struct Sched { u8 pad0[0x27C]; volatile u32 frameCount; } Sched; /* gScheduler: its retrace count */
VIEW(Sched, gScheduler);
#define gScheduler gScheduler_view

/* The game's button bits the dialogs answer to. */
#define BUTTONS_START_A_B 0x7
#define BUTTONS_DPAD_UP_DOWN 0xC00

extern s32 sprintf(char *, const char *, ...);

/* Draws text in dialog [row][col]'s box, a dialog with nothing to answer (MAT3 _DrawPakErrNQA). */
void DrawPakErrNQA(s32 row, s32 col, u8 *text)
{
    Cell *c;

    HAL_SetFont(11);
    TB_SetTextColor(1);
    c = &gPakError[row][col];
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(c->width / 2 + c->left, c->height / 2 + c->top, c->width, c->height, -1, 0, text);
    TB_SetJust(0, 3);
}

/* Draws text in dialog [row][col]'s box above three answers, s1 to s3, the chosen one (sel1, sel2, else the third)
 * in the highlighted font and colour, a pixel higher. */
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
    TB_DrawTextScrollBox(c->width / 2 + c->left, (c->height - 40) / 2 + c->top + 20, c->width, c->height - 40, -1, 0, text);
    fh = TB_GetFontLineHeight();
    x = c->width / 2 + c->left;
    y = c->height + c->top - fh * 3;
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

/* A dialog with nothing to answer closes on START, A or B (MAT3 _ControlPakErrNQA). */
void ControlPakErrNQA(s32 row, s32 col)
{
  Cell *c;
  s32 edges;
  c = &gPakError[row][col];
  edges = c->edges;
  if ((edges & BUTTONS_START_A_B) != 0)
  {
    PlayStartABSound(edges);
    c->done = 1;
  }
}

/* A two-answer dialog: up or down swaps the answer, A takes it (MAT3 _ControlChoice). */
void ControlChoice(s32 row, s32 col)
{
  Cell *c;
  c = &gPakError[row][col];
  if (c->edges & BUTTONS_DPAD_UP_DOWN)
  {
    PlayLRSound(c->edges);
    c->choice1 = !c->choice1;
  }
  if (c->edges & BUTTON_A)
  {
    AudPlayEffect(0x25, 0, 1, 0U);
    c->done = 1;
  }
}

/* Draws text in dialog [row][col]'s box above two answers: the second word of the language's list (lang[25]) and
 * message 236, the chosen one (sel: the first) highlighted. a and b are unused (MAT3 has fixed labels too). */
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
    TB_DrawTextScrollBox(c->width / 2 + c->left, (c->height - 40) / 2 + c->top + 20, c->width, c->height - 40, -1, 0, (u8 *)text);
    fh = TB_GetFontLineHeight();
    x = c->width / 2 + c->left;
    y = c->height + c->top - fh * 3;
    HAL_SetFont(sel ? 11 : 10);
    TB_SetTextColor(sel ? 1 : 22);
    TB_DrawText(x, y - (sel ? 0 : 1) + fh, gText.msg[944 / 4]);
    HAL_SetFont(sel ? 10 : 11);
    TB_SetTextColor(sel ? 22 : 1);
    TB_DrawText(x, y - (sel ? 1 : 0), gText.strings[gText.lang[25]]);
    TB_SetJust(0, 3);
}

/* The no-pak dialog: in a race (outside the front end) it asks whether to go on without the pak (MAT3
 * _DrawPakErrNoPak). */
void DrawPakErrNoPak(s32 row, s32 col)
{
    s32 x;
    s32 y;
    s32 fh;
    u8 buf[256];
    s32 big;

    big = !(gGameState & GS_FRONTEND_MASK);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    TB_Strcpy(buf, gText.msg[416 / 4]);
    TB_Strcat(buf, (u8 *)(big ? gText.msg[412 / 4] : gText.msg[408 / 4]));
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(gPakError[row][col].left + gPakError[row][col].width / 2, gPakError[row][col].top + (gPakError[row][col].height - (big ? 40 : 20)) / 2 + 20, gPakError[row][col].width, gPakError[row][col].height - (big ? 40 : 20),
                       -1, 0, buf);
    if (big) {
        fh = TB_GetFontLineHeight();
        x = gPakError[row][col].left + gPakError[row][col].width / 2;
        y = gPakError[row][col].top + gPakError[row][col].height - fh * 3;
        HAL_SetFont(gPakError[row][col].choice1 ? 11 : 10);
        TB_SetTextColor(gPakError[row][col].choice1 ? 1 : 22);
        TB_DrawText(x, y - (gPakError[row][col].choice1 ? 0 : 1) + fh, gText.msg[944 / 4]);
        HAL_SetFont(gPakError[row][col].choice1 ? 10 : 11);
        TB_SetTextColor(gPakError[row][col].choice1 ? 22 : 1);
        TB_DrawText(x, y - (gPakError[row][col].choice1 ? 1 : 0), gText.strings[gText.lang[25]]);
    }
    TB_SetJust(0, 3);
    HAL_SetFont(11);
}

#define C (gPakError[row][col])
/* The no-controller dialog (MAT3 _DrawPakErrNoController). */
void cs_draw1_DC(s32 row, s32 col)
{
    u8 str[256];

    HAL_SetFont(11);
    TB_SetTextColor(1);
    TB_Strcpy(str, gText.msg[404 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_no_controller);
    TB_Strcat(str, (u8 *)gText.msg[408 / 4]);
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(C.left + C.width / 2, C.top + (C.height - 20) / 2 + 20, C.width, C.height - 20, -1, 0, str);
    TB_SetJust(0, 3);
    HAL_SetFont(11);
}

/* The new-pak dialog: a pak other than the one the player started with (MAT3 _DrawPakErrNewPak). */
void cs_draw3_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, C.alt ? gText.msg[424 / 4] : gText.msg[428 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_new_pak);
    TB_Strcat(str, (u8 *)gText.msg[432 / 4]);
    DrawChoice(row, col, (intptr_t)str, gText.strings[gText.lang[25]], gText.msg[944 / 4], C.choice1);
}

/* The damaged (inconsistent) pak dialog (MAT3 _DrawPakErrInconsistent). */
void cs_drawT0_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[436 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_inconsistent);
    TB_Strcat(str, (u8 *)gText.msg[440 / 4]);
    DrawPakErrNQA(row, col, str);
}

/* The unknown error dialog (MAT3 _DrawPakErrUnknown). */
void cs_drawT1_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[444 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_unknown);
    TB_Strcat(str, (u8 *)gText.msg[408 / 4]);
    DrawPakErrNQA(row, col, str);
}

/* The full pak dialog (MAT3 _DrawPakErrFull). */
void cs_drawT2_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[452 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_full);
    TB_Strcat(str, (u8 *)gText.msg[456 / 4]);
    DrawPakErrNQA(row, col, str);
}

/* The bad data dialog (MAT3 _DrawPakErrBadData). */
void cs_drawT3_DC(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.msg[460 / 4]);
    TB_Strcat(str, (u8 *)pakerr_newlines_bad_data);
    TB_Strcat(str, (u8 *)gText.msg[440 / 4]);
    DrawPakErrNQA(row, col, str);
}

/* The three waiting dialogs (messages 119, 120 and 121): each draws its message and closes itself at its
 * timeout (MAT3 _ControlPakErrTimeout with the draw). */
void cs_wait11_DC(s32 row, s32 col)
{
    if (C.timeout < gScheduler.frameCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.msg[476 / 4]);
}

/* As cs_wait11_DC, with message 120. */
void cs_wait12_DC(s32 row, s32 col)
{
    if (C.timeout < gScheduler.frameCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.msg[480 / 4]);
}

/* As cs_wait11_DC, with message 121. */
void cs_wait13_DC(s32 row, s32 col)
{
    if (C.timeout < gScheduler.frameCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.msg[484 / 4]);
}

/* A three-answer dialog: down and up move the answer round (first, second, third), A takes it. */
void cs_ctl10_DC(Cell *c)
{
    if (c->edges & BUTTON_DOWN) {
        PlayUDSound(c->edges);
        if (c->choice1) {
            c->choice1 = 0;
            c->choice2 = 1;
        } else if (c->choice2) {
            c->choice2 = 0;
        } else {
            c->choice1 = 1;
        }
    }
    if (c->edges & BUTTON_UP) {
        PlayUDSound(c->edges);
        if (c->choice1) {
            c->choice1 = 0;
        } else if (c->choice2) {
            c->choice2 = 0;
            c->choice1 = 1;
        } else {
            c->choice2 = 1;
        }
    }
    if (c->edges & BUTTON_A) {
        AudPlayEffect(37, 0, 1, 0);
        c->done = 1;
    }
}

/* Runs pak row's dialog for this frame (MAT3 ShowPakError): poll, when given, may cancel it (the pak is back). The
 * later of the pak's two dialogs that is up is shown (a closed one that never finished is finished first: it reports
 * done with no choice); its title says the controller; the frame's buttons go to it, and it is drawn by its error.
 * When it is done its answers go to *choice1 and *choice2 and the controllers' active and stick settings it holds
 * are applied (and the controls read again). *done says whether it is. */
void ShowPakError(s32 row, s8 *choice1, s8 *choice2, s8 *done, s32 (*poll)(void))
{
    s32 col;
    s32 i;
    char num[72];

    if (poll != 0 && poll() != 0) {
        CancelPakError(row);
    }
    for (col = 1; col >= 0; col--) {
        if (C.err != 0) {
            break;
        }
        if (C.done == 0) {
            C.done = 1;
            *done = 1;
            *choice2 = 0;
            *choice1 = 0;
            goto apply;
        }
    }
    if (col < 0) {
        return;
    }
    C.edges = gActiveEdges;
    C.levels = gActiveLevels;
    HAL_SetFont(11);
    TB_SetTextColor(1);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    sprintf(num, pak_error_title_format, row + 1);
    TB_DrawText(TB_CenterText(num, C.left + C.width / 2), C.top, num);
    switch (C.err) {
    case 1:
        ControlChoice(row, col);
        cs_draw1_DC(row, col);
        break;
    case 2:
        if (!(gGameState & GS_FRONTEND_MASK)) {
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
        DrawChoice(row, col, (intptr_t)gText.msg[444 / 4], (gText.strings + gText.lang[26])[1], gText.msg[944 / 4], C.choice1);
        break;
    case 8:
        ControlChoice(row, col);
        DrawChoice(row, col, (intptr_t)gText.msg[448 / 4], gText.strings[gText.lang[24]], (gText.strings + gText.lang[24])[1], C.choice1);
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
        DrawChoice3(row, col, (u8 *)gText.msg[472 / 4], gText.msg[944 / 4], gText.strings[gText.lang[26]], (gText.strings + gText.lang[26])[1], C.choice1, C.choice2);
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
        C.err = 0;
        *choice1 = C.choice1;
        *choice2 = C.choice2;
apply:
        for (i = 0; i < 4; i++) {
            SetControllerActive(i, C.contActive[i]);
            SetControllerStickEdges(i, C.contStick[i]);
        }
        UpdateRushControls();
    }
}
