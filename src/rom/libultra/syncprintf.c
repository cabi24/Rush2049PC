/* libultra's debug printing (syncprintf.c): osSyncPrintf and rmonPrintf, which print nothing in the retail
 * library. */
#include "../rom_tu.h"

/* Debug print: nothing in the retail library. */
void osSyncPrintf(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
  int new_var;
  if (!new_var)
  {
  }
  new_var = (int) (1 & 0xFF);
  if (new_var)
  {
  }
}

/* The remote monitor's debug print: nothing in the retail library. */
typedef char *va_list;
void rmonPrintf(const char* fmt, ...) {
    int ans;
    va_list ap;

}

