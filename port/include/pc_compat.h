/* port/include/pc_compat.h: POSIX calls the PC layer and its tests make that mingw-w64 lacks or spells
 * differently, for Windows builds (docs/port/WINDOWS.md). Elsewhere it only names pc_rename_replace. Include it
 * after the system headers. */
#ifndef PC_COMPAT_H
#define PC_COMPAT_H

#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <stdlib.h>

/* The C runtime's environment, which getenv reads (the CRT passes it on to the process's). An empty value removes
 * the variable there. Under their own names, in case a newer mingw-w64 declares setenv. */
static __inline int pc_setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name))
        return 0;
    return _putenv_s(name, value) == 0 ? 0 : -1;
}
static __inline int pc_unsetenv(const char *name) { return _putenv_s(name, "") == 0 ? 0 : -1; }
#define setenv pc_setenv
#define unsetenv pc_unsetenv

/* No permission bits. */
#define mkdir(path, mode) _mkdir(path)

/* rename() fails there when the target exists; POSIX replaces it. */
__declspec(dllimport) int __stdcall MoveFileExA(const char *from, const char *to, unsigned long flags);
static __inline int pc_rename_replace(const char *from, const char *to)
{
    return MoveFileExA(from, to, 0x1 /* MOVEFILE_REPLACE_EXISTING */) ? 0 : -1;
}
#else
#define pc_rename_replace rename
#endif

#endif
