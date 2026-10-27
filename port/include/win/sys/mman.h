/* port/include/win/sys/mman.h: the part of <sys/mman.h> the PC layer and its tests use, on Windows, where
 * mingw-w64 has none (docs/port/WINDOWS.md). On the include path for Windows builds only (CMakeLists.txt).
 *
 * Anonymous private mappings only, which is what the tests' RDRAM windows and port/data use: mmap reserves and
 * commits with VirtualAlloc, at the address asked for when there is one, and fails rather than moving when
 * that range is taken, as MAP_FIXED_NOREPLACE does on Linux (MAP_FIXED is treated the same). munmap releases a
 * whole mapping (VirtualFree needs its start), and mprotect is VirtualProtect. port/ultra/memory.c maps the
 * game's own window, which needs two views of the same memory, with file mappings instead. */
#ifndef PC_WIN_SYS_MMAN_H
#define PC_WIN_SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FIXED_NOREPLACE 0x100000
#define MAP_FAILED ((void *)-1)

/* kernel32, declared here so that the users don't get all of <windows.h> */
__declspec(dllimport) void *__stdcall VirtualAlloc(void *address, size_t size, unsigned long type,
                                                   unsigned long protect);
__declspec(dllimport) int __stdcall VirtualFree(void *address, size_t size, unsigned long type);
__declspec(dllimport) int __stdcall VirtualProtect(void *address, size_t size, unsigned long protect,
                                                   unsigned long *old);

static __inline unsigned long pc_win_protect(int prot)
{
    if (prot & PROT_EXEC)
        return prot & PROT_WRITE ? 0x40 : prot & PROT_READ ? 0x20 : 0x10;   /* PAGE_EXECUTE_* */
    return prot & PROT_WRITE ? 0x04 : prot & PROT_READ ? 0x02 : 0x01;       /* READWRITE, READONLY, NOACCESS */
}

static __inline void *mmap(void *address, size_t size, int prot, int flags, int fd, off_t offset)
{
    void *p;
    (void)offset;
    if (fd != -1 || !(flags & MAP_ANONYMOUS) || size == 0)
        return MAP_FAILED;
    p = VirtualAlloc(address, size, 0x3000 /* MEM_COMMIT | MEM_RESERVE */, pc_win_protect(prot));
    if (p == NULL || (address != NULL && (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) && p != address)) {
        if (p != NULL)
            VirtualFree(p, 0, 0x8000 /* MEM_RELEASE */);
        return MAP_FAILED;
    }
    return p;
}

static __inline int munmap(void *address, size_t size)
{
    (void)size;
    return VirtualFree(address, 0, 0x8000 /* MEM_RELEASE */) ? 0 : -1;
}

static __inline int mprotect(void *address, size_t size, int prot)
{
    unsigned long old;
    return VirtualProtect(address, size, pc_win_protect(prot), &old) ? 0 : -1;
}

#endif
