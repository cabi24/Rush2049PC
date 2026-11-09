#include "game/game.h"

/* PC: printf at (x, y), formatted by the host as TB_Sprintf does. The N64 version hands fcvt the address of
 * its stack arguments as an IDO va_list, and its format (arg2) is a pointer every caller passes
 * (docs/port/PROTOTYPES.md). */
int vsprintf(char *, const char *, __builtin_va_list);
void TB_Printf(s16 arg0, s16 arg1, const char *arg2, ...) {
    s8 buf[0x100];
    __builtin_va_list ap;

    __builtin_va_start(ap, arg2);
    vsprintf((char *) buf, arg2, ap);
    __builtin_va_end(ap);
    TB_DrawText(arg0, (s32) arg1, buf);
}

