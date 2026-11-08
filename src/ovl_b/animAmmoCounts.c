/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Runtime image B: 0x80393518..0x803936A8. */
typedef signed char s8;
typedef signed short s16;
typedef unsigned char u8;
typedef struct Player952 {
    u8 before_kind[0x110 - 0x0]; struct pc_visual visuals[21]; u8 before_kind_308[0x380 - 0x308]; void *input;
    s8 kind;
    s8 lives;
    u8 tail[0x32];
} Player952;
typedef struct TextPosition { s16 x, y; } TextPosition;
extern s16 gNumWindows;
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern TextPosition ammoPos[4][4];
extern char ammo_count_format[];
extern void HAL_SetTextDepth(float);
extern int HAL_SetFont(int);
extern void TB_SetJust(unsigned int, unsigned int);
extern void TB_Sprintf(char *, char *, ...);
extern void TB_SetTextColor(int);
extern void TB_DrawText(s16, s16, char *);
int animAmmoCounts(void *callback_context)
{
    s16 i;
    s16 x, y;
    char text[12];
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(10);
    TB_SetJust(1, 1);
    for (i = 0; i < gNumWindows; i++) {
        if (i < gNumWindows && PC_CAR_KIND(i) != 8) {
            x = ammoPos[gNumWindows - 1][i].x;
            y = ammoPos[gNumWindows - 1][i].y;
            if (PC_CAR_LIVES(i) >= 0) {
                TB_Sprintf(text, ammo_count_format, PC_CAR_LIVES(i));
                TB_SetTextColor(0);
                TB_DrawText(x + 1, y + 1, text);
                TB_SetTextColor(1);
                TB_DrawText(x, y, text);
            }
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
