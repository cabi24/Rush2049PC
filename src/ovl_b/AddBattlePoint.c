#include "game/game.h"

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

VIEW(s8[], gOptions);
#define gOptions gOptions_view

VIEW(Event[], battlePoint);
#define battlePoint battlePoint_view

VIEW(s32[][4][2], wrongWayPos);
#define wrongWayPos wrongWayPos_view

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
    event->from_x = wrongWayPos[gNumWindows - 1][column][0];
    event->from_y = wrongWayPos[gNumWindows - 1][column][1];
    event->to_x = fragPos[gNumWindows - 1][row][0];
    event->to_y = fragPos[gNumWindows - 1][row][1];
}
