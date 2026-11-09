/* TB_Printf: draw formatted text (MAT3 TB_text.c). */
#include "game/game.h"

int vsprintf(char *, const char *, __builtin_va_list);

/* Draws the printf-style format and its arguments at (x, y), up to 255 bytes. The host's vsprintf formats it (the N64's
 * own handed its stack arguments to the game's formatter as an IDO va_list; docs/port/PROTOTYPES.md). */
void TB_Printf(s16 x, s16 y, const char *format, ...) {
    s8 buf[0x100];
    __builtin_va_list ap;

    __builtin_va_start(ap, format);
    vsprintf((char *) buf, format, ap);
    __builtin_va_end(ap);
    TB_DrawText(x, (s32) y, buf);
}
