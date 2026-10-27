/* port/ultra/n64mem.c: libultra's bzero, bcopy and bcmp, with their 32-bit lengths. CMakeLists.txt renames the
 * game's calls to these (the host's take a size_t, which the game's own prototypes don't declare). On its own
 * so that test programs linking game C can link it too. */
#include <stdint.h>
#include <string.h>

void n64_bzero(void *p, uint32_t n) { memset(p, 0, n); }
void n64_bcopy(const void *src, void *dst, uint32_t n) { memmove(dst, src, n); }
int n64_bcmp(const void *a, const void *b, uint32_t n) { return memcmp(a, b, n) != 0; }
