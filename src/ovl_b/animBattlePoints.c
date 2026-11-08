/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image B: 0x803936A8..0x80393B34. Draw player counters and consume the
 * four-record HUD event ring, applying expired deltas and drawing younger events.
 * Native reconstruction; original names and whole-function arcade ancestry unknown.
 * Genuine callback ABI: one opaque pointer argument, return 1. All callees external.
 * Source shaping: the real 32-byte text buffer precedes i/x/y/event. IDO lays
 * these locals downwards: text at sp+112, four 4-byte homes below it, total
 * frame 144 bytes. The prior reverse order placed text at sp+96 in the same
 * frame. No new locals, padding, compiled-out reads or helper bodies were added.
 * Buffer length is inferred, not recovered source. Formatting must fit in 32
 * bytes including NUL; counts/selectors/event indices retain native bounds.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef struct Player { u8 unknown000[0x110 - 0x0]; struct pc_visual visuals[21]; u8 unknown000_308[0x380 - 0x308]; void *input; u8 unknown000_384[0x3A3 - 0x384]; s8 remaining; u8 unknown3A4[0x14]; } Player;
typedef struct Event {
    s8 active, age, column, row;
    f32 from_x, from_y, to_x, to_y;
    s8 delta;
    u8 unknown15[3];
} Event;
/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct Texts { char *before108[108 / 4]; char *same_player; char *other_player; } Texts;
typedef struct LanguageState { s32 unknown0; Texts *texts; } LanguageState;
/* PC: the data is the player's profile record (save.c's SaveSlot), 64 bytes, its name at +0x20 (PROFILE_OFS,
 * docs/port/POINTERS.md 9.10). */
typedef struct NameData { u8 before_name[0x20]; char name[1]; } NameData;
typedef struct NameRef { NameData *data; } NameRef;
typedef struct PlayerInfo { NameRef *name_ref; u8 unknown04[72]; } PlayerInfo;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern Event battlePoint[4];
extern s16 gNumPlayers, gNumWindows;
extern s32 fragPos[4][4][2], wrongWayPos[4][4][2];
extern char battle_points_format[], battle_point_lost_text[], battle_point_won_text[];
extern LanguageState gText;
extern PlayerInfo gPlayers_save[];
extern s8 gCarTeam[], gOutOfTime;
extern s32 playerTextColor[], gPauseState;
extern void HAL_SetTextDepth(f32);
extern s32 HAL_SetFont(s32);
extern void TB_SetJust(unsigned int, unsigned int);
extern int sprintf(char *, const char *, ...);
extern void TB_Sprintf(char *, char *, ...);
extern void TB_SetTextColor(s32);
extern void TB_DrawText(s16, s16, void *);
extern void TB_DrawTextScrollBox(s16, s16, s16, s16, s16, s16, u8 *);
extern void RecordBattlePoints(s8, s8, s8);

s32 animBattlePoints(void *callback_context)
{
    char text[32];
    s32 i, x, y;
    Event *event;
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(10);
    TB_SetJust(1, 1);
    for (i = 0; i < gNumPlayers; i++) {
        x = fragPos[gNumWindows - 1][i][0];
        y = fragPos[gNumWindows - 1][i][1];
        sprintf(text, battle_points_format, PC_CAR_POINTS(i));
        TB_SetTextColor(0);
        TB_DrawText(x + 1, y + 1, text);
        TB_SetTextColor(1);
        TB_DrawText(x, y, text);
    }
    for (i = 0; i < 4; i++) {
        event = &battlePoint[i];
        if (event->active) {
            if (event->age > 40) {
                event->active = 0;
                if (event->delta > 0) {
                    RecordBattlePoints(event->row, event->column, event->delta);
                }
                PC_CAR_POINTS(event->row) += event->delta;
            } else {
                x = ((event->to_x - event->from_x) / 40.0f) * event->age + event->from_x;
                y = ((event->to_y - event->from_y) / 40.0f) * event->age + event->from_y;
                if (event->column == event->row) {
                    TB_SetTextColor(1);
                    TB_DrawText(x, y, battle_point_lost_text);
                    x = wrongWayPos[gNumWindows - 1][event->column][0];
                    y = wrongWayPos[gNumWindows - 1][event->column][1];
                    if (gNumPlayers >= 3) {
                        HAL_SetFont(11);
                        TB_SetTextColor(0);
                        TB_DrawText(x + 1, y + 1, gText.texts->same_player);
                        TB_SetTextColor(1);
                        TB_DrawText(x, y, gText.texts->same_player);
                    }
                } else {
                    TB_SetTextColor(1);
                    TB_DrawText(x, y, battle_point_won_text);
                    x = wrongWayPos[gNumWindows - 1][event->column][0];
                    y = wrongWayPos[gNumWindows - 1][event->column][1];
                    TB_Sprintf(text, gText.texts->other_player,
                                 gPlayers_save[event->row].name_ref->data->name);
                    if (gNumPlayers >= 3) {
                        HAL_SetFont(11);
                        TB_SetTextColor(0);
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
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
