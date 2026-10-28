/* port/ultra/tests/test_absfix.c: the RELRO pages absfix.c opens and protects again, for the page sizes aarch64
 * kernels use (4, 16 and 64 KiB) and a RELRO end aligned only to 4 KiB, as Ubuntu 22.04's binutils link it. */
#include <stdint.h>
#include <stdio.h>

void pc_relro_pages(uintptr_t start, uintptr_t size, uintptr_t page, uintptr_t *lo, uintptr_t *hi);

static int failures;

static void check(uintptr_t start, uintptr_t size, uintptr_t page, uintptr_t want_lo, uintptr_t want_hi)
{
    uintptr_t lo, hi;
    pc_relro_pages(start, size, page, &lo, &hi);
    if (lo != want_lo || hi != want_hi) {
        printf("FAIL relro %#lx+%#lx, page %#lx: [%#lx, %#lx), want [%#lx, %#lx)\n", (unsigned long)start,
               (unsigned long)size, (unsigned long)page, (unsigned long)lo, (unsigned long)hi, (unsigned long)want_lo,
               (unsigned long)want_hi);
        failures++;
    }
}

int main(void)
{
    /* a RELRO end at a 4 KiB boundary, where .data starts */
    check(0x3cd000, 0x2c000, 0x1000, 0x3cd000, 0x3f9000);
    check(0x3cd000, 0x2c000, 0x4000, 0x3cc000, 0x3f8000);   /* not 0x3fc000: .data's first 12 KiB stay writable */
    check(0x3cd000, 0x2c000, 0x10000, 0x3c0000, 0x3f0000);
    /* an end inside a page */
    check(0x2fefc0, 0x21040, 0x4000, 0x2fc000, 0x320000);
    /* a segment inside one page: nothing to protect */
    check(0x401100, 0x200, 0x4000, 0x400000, 0x400000);
    printf("absfix: %s\n", failures ? "FAILED" : "ok");
    return failures != 0;
}
