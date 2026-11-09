/* AddBattlePointInternal: a battle point won or lost, and its marker flying across the HUD (MAT3 hudextra.c). */
#include "game/game.h"

/* A battle point's marker (battlePoint[4]): it flies from the victim's place on the HUD to the scorer's. */
typedef struct Event {
    s8 active;
    s8 age;
    s8 column;
    s8 row;
    f32 from_x;
    f32 from_y;
    f32 to_x;
    f32 to_y;
    s8 delta;
    u8 unknown15[3];
} Event;

/* Records `delta` points for car `row` against car `column` (RecordBattlePoints for a loss); a lost point takes one
 * off the car (none below 0) when the option for it (gOptions[40]) is on, else stops there. Starts a marker flying
 * from `column`'s place on the HUD (wrongWayPos) to `row`'s score (fragPos), in the next of four marker slots. */
void AddBattlePointInternal(s8 row, s8 column, s8 delta) {
    Event *event;
    s32 index;
    if (delta < 0) {
        RecordBattlePoints(row, column, delta);
        if (gOptions[40] == 0) {
            return;
        }
        if (PC_CAR_POINTS(row) <= 0) {
            PC_CAR_POINTS(row) = 0;
            return;
        }
    }
    index = battlePointCnt++;
    if (battlePointCnt >= 4) {
        battlePointCnt = 0;
    }
    event = &battlePoint[index];
    event->active = 1;
    event->age = 0;
    event->row = row;
    event->column = column;
    event->delta = delta;
    event->from_x = wrongWayPos[gNumWindows - 1][column].x;
    event->from_y = wrongWayPos[gNumWindows - 1][column].y;
    event->to_x = fragPos[gNumWindows - 1][row][0];
    event->to_y = fragPos[gNumWindows - 1][row][1];
}
