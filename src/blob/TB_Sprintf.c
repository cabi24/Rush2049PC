/* TB_Sprintf: format text into a buffer (MAT3 TB_text.c). */
#include "game/game.h"

int vsprintf(char *, const char *, __builtin_va_list);

/* sprintf: the printf-style format and its arguments into buffer. The host's vsprintf formats it (the N64's own
 * passed its stack arguments as an IDO va_list). */
void TB_Sprintf(char *buffer, const char *format, ...)
{
  __builtin_va_list ap;
  __builtin_va_start(ap, format);
  vsprintf(buffer, format, ap);
  __builtin_va_end(ap);
}
