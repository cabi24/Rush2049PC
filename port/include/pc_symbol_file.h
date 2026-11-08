/* port/include/pc_symbol_file.h: a game symbol's N64 address by its name, from the game's symbol file
 * (symbol_addrs.us.txt, lines `name = 0xADDRESS; // ...`), for the checks that need one the data loader's tables don't
 * hold (port/data/datacheck.c, port/assets/tests/assetcheck.c): data no PC declaration describes, an image's load
 * address. The program itself never reads the symbol file; it asks the loader's tables (pc_data_addr_of). */
#ifndef PC_SYMBOL_FILE_H
#define PC_SYMBOL_FILE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The address the symbol file at `path` gives `name` (its first line for it), or 0 if none or no file. */
static inline uint32_t pc_symbol_file_addr(const char *path, const char *name)
{
    FILE *f = fopen(path, "r");
    char line[512], key[128];
    unsigned int addr;
    uint32_t found = 0;
    while (f && !found && fgets(line, sizeof line, f))
        if (sscanf(line, " %127[A-Za-z0-9_] = 0x%x", key, &addr) == 2 && strcmp(key, name) == 0)
            found = addr;
    if (f)
        fclose(f);
    return found;
}

#endif
