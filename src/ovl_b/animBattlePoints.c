/* animBattlePoints: the battle points and the points flying, a text blit's draw function (MAT3 hudextra.c;
 * runtime image B). */
#include "game/game.h"

#define POINT_FLIGHT 40          /* steps (each a 30th of a second) a battle point takes to fly */
#define POINT_LOST_TEXT 27       /* gText.single: the "point lost" message */
#define POINT_WON_FORMAT 28      /* gText.single: the "point from ..." message's format */



extern int sprintf(char *, const char *, ...);

/* Draws every player's points at fragPos (white, black shadow), and each point flying (AddBattlePointInternal) from
 * the scorer towards the scored (or its own, a point lost) over POINT_FLIGHT steps, with a message in the victim's
 * window with 3 players or more; at the end of its flight the point is added (RecordBattlePoints for a point won).
 * The flight stops while out of time or paused. Returns 1. */
s32 animBattlePoints(void *callback_context)
{
    char text[32];
    s32 i, x, y;
    BattlePointEvent *event;
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(10);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    for (i = 0; i < gNumPlayers; i++) {
        x = fragPos[gNumWindows - 1][i][0];
        y = fragPos[gNumWindows - 1][i][1];
        sprintf(text, battle_points_format, PC_CAR_POINTS(i));
        TB_SetTextColor(kTB_BlackText);
        TB_DrawText(x + 1, y + 1, text);
        TB_SetTextColor(kTB_WhiteText);
        TB_DrawText(x, y, text);
    }
    for (i = 0; i < 4; i++) {
        event = &battlePoint[i];
        if (event->active) {
            if (event->age > POINT_FLIGHT) {
                event->active = 0;
                if (event->delta > 0) {
                    RecordBattlePoints(event->row, event->column, event->delta);
                }
                PC_CAR_POINTS(event->row) += event->delta;
            } else {
                x = ((event->to_x - event->from_x) / (f32) POINT_FLIGHT) * event->age + event->from_x;
                y = ((event->to_y - event->from_y) / (f32) POINT_FLIGHT) * event->age + event->from_y;
                if (event->column == event->row) {
                    TB_SetTextColor(kTB_WhiteText);
                    TB_DrawText(x, y, battle_point_lost_text);
                    x = wrongWayPos[gNumWindows - 1][event->column].x;
                    y = wrongWayPos[gNumWindows - 1][event->column].y;
                    if (gNumPlayers >= 3) {
                        HAL_SetFont(11);
                        TB_SetTextColor(kTB_BlackText);
                        TB_DrawText(x + 1, y + 1, gText.single[POINT_LOST_TEXT]);
                        TB_SetTextColor(kTB_WhiteText);
                        TB_DrawText(x, y, gText.single[POINT_LOST_TEXT]);
                    }
                } else {
                    TB_SetTextColor(kTB_WhiteText);
                    TB_DrawText(x, y, battle_point_won_text);
                    x = wrongWayPos[gNumWindows - 1][event->column].x;
                    y = wrongWayPos[gNumWindows - 1][event->column].y;
                    TB_Sprintf(text, gText.single[POINT_WON_FORMAT],
                                 (*gPlayers_save[event->row].profile)->name);
                    if (gNumPlayers >= 3) {
                        HAL_SetFont(11);
                        TB_SetTextColor(kTB_BlackText);
                        TB_DrawTextScrollBox(x + 1, y + 1, 144, 110, -1, 0, (u8 *)text);
                        TB_SetTextColor(playerTextColor[gCarTeam[event->row]]);
                        TB_DrawTextScrollBox(x, y, 144, 110, -1, 0, (u8 *)text);
                    }
                }
                if (!gOutOfTime && !gPauseState) {
                    PC_EACH30(event->age++;)
                }
            }
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
