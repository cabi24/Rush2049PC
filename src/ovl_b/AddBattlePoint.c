/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image B, stream 0xB6FEC4, 0x803914B4..0x80391640 (396 bytes).
 * Queue one 24-byte HUD event. Negative deltas update the real score table
 * before optional animation, and nonpositive player counters are cleared.
 * The consumer at B:803936A8 confirms the four records and every field.
 * N64-specific source reconstruction; no identified arcade equivalent.
 * Ordinary three-signed-byte O32 interface; standalone IDO 5.3 -O3.
 * Valid player/column/ring indices and backed coordinate-table rows required.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef int s32;
typedef float f32;

typedef struct Player {
    u8 unknown000[0x110 - 0x0]; struct pc_visual visuals[21]; u8 unknown000_308[0x380 - 0x308]; void *input; u8 unknown000_384[0x3A3 - 0x384];
    s8 remaining;
    u8 unknown3A4[0x14];
} Player;

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

extern void RecordBattlePoints(s8 row, s8 column, s8 delta);
/* The game options, a byte each (Settings): [40] the battle's self-hit option (MAT3 trackscr row 11). */
extern s8 gOptions[];
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 battlePointCnt;
extern Event battlePoint[];
extern s16 gNumWindows;
extern s32 wrongWayPos[][4][2];
extern s32 fragPos[][4][2];

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
