/* The Controller Pak error dialogs (MAT3 pakerr.c): a box per controller (gPakError[pak][id], two a pak) that
 * says what is wrong with the pak (none, a new one, damaged, full, bad data ...) and takes the player's answer.
 * ShowPakError draws and runs them each frame; SetPakError (elsewhere) opens one. While a dialog is up the
 * controllers it lists are made active (and their sticks drive the d-pad) as it asks. */
#include "game/game.h"




/* The game's button bits the dialogs answer to. */
#define BUTTONS_START_A_B 0x7
#define BUTTONS_DPAD_UP_DOWN 0xC00

extern s32 sprintf(char *, const char *, ...);

/* Draws text in dialog [row][col]'s box, a dialog with nothing to answer (MAT3 _DrawPakErrNQA). */
void DrawPakErrNQA(s32 row, s32 col, u8 *text)
{
    PakErrorState *c;

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
    PakErrorState *c;
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
  PakErrorState *c;
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
  PakErrorState *c;
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
    PakErrorState *c;
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
    TB_DrawText(x, y - (sel ? 0 : 1) + fh, gText.single[236]);
    HAL_SetFont(sel ? 10 : 11);
    TB_SetTextColor(sel ? 22 : 1);
    TB_DrawText(x, y - (sel ? 1 : 0), gText.taba[gText.tab[25]]);
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
    TB_Strcpy(buf, gText.single[104]);
    TB_Strcat(buf, (u8 *)(big ? gText.single[103] : gText.single[102]));
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(gPakError[row][col].left + gPakError[row][col].width / 2, gPakError[row][col].top + (gPakError[row][col].height - (big ? 40 : 20)) / 2 + 20, gPakError[row][col].width, gPakError[row][col].height - (big ? 40 : 20),
                       -1, 0, buf);
    if (big) {
        fh = TB_GetFontLineHeight();
        x = gPakError[row][col].left + gPakError[row][col].width / 2;
        y = gPakError[row][col].top + gPakError[row][col].height - fh * 3;
        HAL_SetFont(gPakError[row][col].choice1 ? 11 : 10);
        TB_SetTextColor(gPakError[row][col].choice1 ? 1 : 22);
        TB_DrawText(x, y - (gPakError[row][col].choice1 ? 0 : 1) + fh, gText.single[236]);
        HAL_SetFont(gPakError[row][col].choice1 ? 10 : 11);
        TB_SetTextColor(gPakError[row][col].choice1 ? 22 : 1);
        TB_DrawText(x, y - (gPakError[row][col].choice1 ? 1 : 0), gText.taba[gText.tab[25]]);
    }
    TB_SetJust(0, 3);
    HAL_SetFont(11);
}

#define C (gPakError[row][col])
/* The no-controller dialog (MAT3 _DrawPakErrNoController). */
void DrawPakErrNoController(s32 row, s32 col)
{
    u8 str[256];

    HAL_SetFont(11);
    TB_SetTextColor(1);
    TB_Strcpy(str, gText.single[101]);
    TB_Strcat(str, (u8 *)pakerr_newlines_no_controller);
    TB_Strcat(str, (u8 *)gText.single[102]);
    TB_SetJust(1, 1);
    TB_DrawTextScrollBox(C.left + C.width / 2, C.top + (C.height - 20) / 2 + 20, C.width, C.height - 20, -1, 0, str);
    TB_SetJust(0, 3);
    HAL_SetFont(11);
}

/* The new-pak dialog: a pak other than the one the player started with (MAT3 _DrawPakErrNewPak). */
void DrawPakErrNewPak(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, C.rumble ? gText.single[106] : gText.single[107]);
    TB_Strcat(str, (u8 *)pakerr_newlines_new_pak);
    TB_Strcat(str, (u8 *)gText.single[108]);
    DrawChoice(row, col, (intptr_t)str, gText.taba[gText.tab[25]], gText.single[236], C.choice1);
}

/* The damaged (inconsistent) pak dialog (MAT3 _DrawPakErrInconsistent). */
void DrawPakErrInconsistent(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.single[109]);
    TB_Strcat(str, (u8 *)pakerr_newlines_inconsistent);
    TB_Strcat(str, (u8 *)gText.single[110]);
    DrawPakErrNQA(row, col, str);
}

/* The unknown error dialog (MAT3 _DrawPakErrUnknown). */
void DrawPakErrUnknown(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.single[111]);
    TB_Strcat(str, (u8 *)pakerr_newlines_unknown);
    TB_Strcat(str, (u8 *)gText.single[102]);
    DrawPakErrNQA(row, col, str);
}

/* The full pak dialog (MAT3 _DrawPakErrFull). */
void DrawPakErrFull(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.single[113]);
    TB_Strcat(str, (u8 *)pakerr_newlines_full);
    TB_Strcat(str, (u8 *)gText.single[114]);
    DrawPakErrNQA(row, col, str);
}

/* The bad data dialog (MAT3 _DrawPakErrBadData). */
void DrawPakErrBadData(s32 row, s32 col)
{
    u8 str[256];

    TB_Strcpy(str, gText.single[115]);
    TB_Strcat(str, (u8 *)pakerr_newlines_bad_data);
    TB_Strcat(str, (u8 *)gText.single[110]);
    DrawPakErrNQA(row, col, str);
}

/* The three waiting dialogs (messages 119, 120 and 121): each draws its message and closes itself at its
 * timeout (MAT3 _ControlPakErrTimeout with the draw). */
void DrawPakErrReread(s32 row, s32 col)
{
    if (C.timeout < gScheduler.retraceCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.single[119]);
}

/* As DrawPakErrReread, with message 120. */
void DrawPakErrRepairFailed(s32 row, s32 col)
{
    if (C.timeout < gScheduler.retraceCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.single[120]);
}

/* As DrawPakErrReread, with message 121. */
void DrawPakErrRepaired(s32 row, s32 col)
{
    if (C.timeout < gScheduler.retraceCount) C.done = 1;
    DrawPakErrNQA(row, col, (u8 *)gText.single[121]);
}

/* A three-answer dialog: down and up move the answer round (first, second, third), A takes it. */
void ControlChoice3(PakErrorState *c)
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

/* Runs pak row's dialog for this frame (MAT3 ShowPakError): poll, when given, is asked about the pak (pak_removed,
 * CancelFunc: MAT3 passes it the pak, as the N64 leaves row in its first argument register) and may cancel the
 * dialog. The later of the pak's two dialogs that is up is shown (a closed one that never finished is finished first:
 * it reports done with no choice); its title says the controller; the frame's buttons go to it, and it is drawn by its
 * error. When it is done its answers go to *choice1 and *choice2 and the controllers' active and stick settings it
 * holds are applied (and the controls read again). *done says whether it is. */
void ShowPakError(s32 row, s8 *choice1, s8 *choice2, s8 *done, s32 (*poll)(s32))
{
    s32 col;
    s32 i;
    char num[72];

    if (poll != 0 && poll(row) != 0) {
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
        DrawPakErrNoController(row, col);
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
        DrawPakErrNewPak(row, col);
        break;
    case 4:
        ControlPakErrNQA(row, col);
        DrawPakErrInconsistent(row, col);
        break;
    case 15:
        ControlPakErrNQA(row, col);
        DrawPakErrUnknown(row, col);
        break;
    case 5:
    case 7:
        ControlChoice(row, col);
        DrawChoice(row, col, (intptr_t)gText.single[111], (gText.taba + gText.tab[26])[1], gText.single[236], C.choice1);
        break;
    case 8:
        ControlChoice(row, col);
        DrawChoice(row, col, (intptr_t)gText.single[112], gText.taba[gText.tab[24]], (gText.taba + gText.tab[24])[1], C.choice1);
        break;
    case 6:
        ControlChoice(row, col);
        DrawPakErrFull(row, col);
        break;
    case 9:
        ControlPakErrNQA(row, col);
        DrawPakErrBadData(row, col);
        break;
    case 10:
        ControlChoice3(&C);
        DrawChoice3(row, col, (u8 *)gText.single[118], gText.single[236], gText.taba[gText.tab[26]], (gText.taba + gText.tab[26])[1], C.choice1, C.choice2);
        break;
    case 11:
        DrawPakErrReread(row, col);
        break;
    case 12:
        DrawPakErrRepairFailed(row, col);
        break;
    case 13:
        DrawPakErrRepaired(row, col);
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
