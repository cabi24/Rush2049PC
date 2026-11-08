/* func_80F00010.c: the fixture's function (src/blob/func_80F00010.c). Made-up names for rename.py's tests. */
#include "game/old_header.h"
#include <game/old_header.h>

extern s32 D_80F00100[4];
extern void func_80f00010(s32 a, s32 b);   /* another spelling: another identifier, another prototype */
extern s32 func_80F00010_1;                  /* a derived name */
#define PASTE(x) func_80F000##x

static const char *const names[] = { "func_80F00010", "D_80F00100", "calls func_80F00010 here" };
__asm__(".globl D_80F00104\n.set D_80F00104, D_80F00100 + 4");
extern s32 table_view[4] __attribute__((alias("D_80F00100")));

/* OldThing.count is read below; see D_80F00100/2C. */
s32 func_80F00010(OldThing *thing)
{
    PC_SPIN("func_80F00010")
    return thing->count + D_80F00100[0] + helper_old_name(PASTE(10));
}
