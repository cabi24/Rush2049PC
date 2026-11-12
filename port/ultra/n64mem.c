/* port/ultra/n64mem.c: libultra's bzero, bcopy and bcmp, with their 32-bit lengths, and the hook of
 * pc_n64_identity. CMakeLists.txt renames the game's calls to these (the host's take a size_t, which the game's own
 * prototypes don't declare). On its own so that test programs linking game C can link it too. */
#include <stdint.h>
#include <string.h>

void n64_bzero(void *p, uint32_t n) { memset(p, 0, n); }
void n64_bcopy(const void *src, void *dst, uint32_t n) { memmove(dst, src, n); }
int n64_bcmp(const void *a, const void *b, uint32_t n) { return memcmp(a, b, n) != 0; }

/* pc_prelude.h's pc_n64_identity. port/ultra/statehash.c, where it is linked, sets the hook from a constructor (the
 * hash's tables name own storage too); without it (the test programs that link game C) it is the pointer's low half.
 * A hook rather than a weak definition: mingw's linker doesn't take a weak definition from an archive
 * (test_assets_save.exe, CI's windows-cross). */
unsigned int (*pc_n64_identity_hook)(const void *p);

unsigned int pc_n64_identity(const void *p)
{
    if (pc_n64_identity_hook)
        return pc_n64_identity_hook(p);
    return (unsigned int)(uintptr_t)p;
}
