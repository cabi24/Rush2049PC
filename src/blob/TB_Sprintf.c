#include "game/game.h"

/* PC: sprintf(buf, fmt, ...). The N64 version passes the address of its stack arguments as an IDO va_list;
 * natively the arguments need a real va_list, and buf/fmt are pointers. The host formats them. */
int vsprintf(char *, const char *, __builtin_va_list);
void TB_Sprintf(char *arg0, const char *arg1, ...)
{
  __builtin_va_list ap;
  __builtin_va_start(ap, arg1);
  vsprintf(arg0, arg1, ap);
  __builtin_va_end(ap);
}

