typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
extern s8 gInited_HALTime;
intptr_t gOPPFuncs[10];
/* The table holds function pointers. On the N64 its end is the next variable, yf (+40); on the
 * PC the slots are 8 bytes and the table ends at its own last element (docs/port/PROTOTYPES.md). */
#define yf gOPPFuncs[10]
void HAL_InitTime(void)
{
  s32 i;
  if (gInited_HALTime == 0) {
    gInited_HALTime = 1;
    for (i = 0; i < 10; i++)
      gOPPFuncs[i] = 0;
  }
}
void HAL_InstallOncePerFrameFunc(intptr_t arg0)
{
  intptr_t *p;
  if (gInited_HALTime == 0) {
    HAL_InitTime();
  }
  for (p = gOPPFuncs; ; ) {
    if (*p == 0) { *p = arg0; break; }
    p++;
    if (p == &yf) break;
  }
}
