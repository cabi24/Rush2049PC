/* port/ultra/castcheck.c: a RUSH_CHECK_CASTS build (cmake -DRUSH_CHECK_CASTS=ON) stops here when game C stores a
 * host address that doesn't fit in 32 bits as an N64 address (PC_N64W, port/include/pc_prelude.h). Only the
 * RDRAM window, at host 0x80000000, fits; own-storage data, the stack, host functions and malloc are above 4 GB.
 * docs/port/CASTS.md. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ultra.h"

void pc_cast_fail(const char *file, int line, uintptr_t value)
{
    fprintf(stderr, "rush2049pc: RUSH_CHECK_CASTS: %s:%d stores host address 0x%llx as a 32-bit N64 address "
                    "(N64 thread %s)\n", file, line, (unsigned long long)value, pc_os_thread_name());
    pc_print_backtrace(1);
    fflush(NULL);
    abort();
}
