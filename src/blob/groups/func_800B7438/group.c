typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
#ifndef TARGET_PC
typedef unsigned int uintptr_t;
typedef s32 intptr_t;
#endif
extern s8 D_801147C4;
intptr_t D_801551E8[10];
#ifdef TARGET_PC
/* The table holds function pointers. On the N64 its end is the next variable, D_80155210 (+40); on the
 * PC the slots are 8 bytes and the table ends at its own last element (docs/port/PROTOTYPES.md). */
#define D_80155210 D_801551E8[10]
#else
extern s32 D_80155210;
#endif
void func_800B73E4(void)
{
  s32 i;
  if (D_801147C4 == 0) {
    D_801147C4 = 1;
    for (i = 0; i < 10; i++)
      D_801551E8[i] = 0;
  }
}
void func_800B7438(intptr_t arg0)
{
  intptr_t *p;
  if (D_801147C4 == 0) {
    func_800B73E4();
  }
  for (p = D_801551E8; ; ) {
    if (*p == 0) { *p = arg0; break; }
    p++;
    if (p == &D_80155210) break;
  }
}
void input_init_flag_get(void) { void (**p)(void); if (D_801147C4 == 0) { func_800B73E4(); } p = (void (**)(void)) &D_801551E8; do { if (*p != 0) { (*p)(); } p++; } while (p != (void (**)(void)) &D_80155210); }
