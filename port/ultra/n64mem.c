/* port/ultra/n64mem.c: libultra's bzero, bcopy and bcmp, with their 32-bit lengths, and (RUSH_NOFIXED) the window's
 * host address the game C translates with. CMakeLists.txt renames the game's calls to these (the host's take a
 * size_t, which the game's own prototypes don't declare). On its own so that test programs linking game C can link
 * it too. */
#include <stdint.h>
#include <string.h>

void n64_bzero(void *p, uint32_t n) { memset(p, 0, n); }
void n64_bcopy(const void *src, void *dst, uint32_t n) { memmove(dst, src, n); }
int n64_bcmp(const void *a, const void *b, uint32_t n) { return memcmp(a, b, n) != 0; }

#ifdef RUSH_NOFIXED
/* The RDRAM window's host address, which the game C's translations between 32-bit N64 addresses and host pointers
 * read (port/include/pc_prelude.h, docs/port/NOFIXED.md 2.3): port/ultra's pc_mem_init sets it to the window, data.c's
 * section pc_window. Until then, and in a test program that maps a window of its own at host 0x80000000 and links
 * game C, it is that address. Here for the reason the functions above are. */
uint8_t *pc_window_base = (uint8_t *)(uintptr_t)0x80000000u;
#endif

/* pc_prelude.h's pc_n64_identity. port/ultra/statehash.c, where it is linked, sets the hook from a constructor (the
 * hash's tables name own storage too); without it (the test programs that link game C) it is PC_N64A's value, a
 * window pointer's N64 address. A hook rather than a weak definition: mingw's linker doesn't take a weak definition
 * from an archive (test_assets_save.exe, CI's windows-cross). */
unsigned int (*pc_n64_identity_hook)(const void *p);

unsigned int pc_n64_identity(const void *p)
{
    if (pc_n64_identity_hook)
        return pc_n64_identity_hook(p);
#ifdef RUSH_NOFIXED
    return p ? 0x80000000u + (unsigned int)((uintptr_t)p - (uintptr_t)pc_window_base) : 0u;
#else
    return (unsigned int)(uintptr_t)p;
#endif
}
