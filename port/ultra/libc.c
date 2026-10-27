/* port/ultra/libc.c: the boot segment's libc pieces the game calls under other names (docs/port/ULTRA.md).
 * Kept apart from the C library's headers: <stdlib.h> declares a different fcvt. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ultra.h"

/* The boot segment's sprintf under its address name (overlay A's menus call it so). */
int func_80004990(char *out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsprintf(out, fmt, ap);
    va_end(ap);
    return n;
}

/* lib_34a0's formatter core (historical label fcvt): (out, format, IDO va_list). fcvt_wrapper passes a
 * pointer into its own stack frame for the arguments, which a native caller doesn't have; until that caller
 * uses a va_list (src/blob/fcvt_wrapper.c), the format is copied unformatted. */
int fcvt(char *out, const char *fmt, char *args)
{
    static int warned;
    (void)args;
    if (!warned++)
        pc_log("fcvt (IDO vsprintf core) called: format copied unformatted: \"%s\"", fmt);
    strcpy(out, fmt);
    return (int)strlen(out);
}
