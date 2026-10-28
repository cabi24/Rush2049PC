/* port/ultra/absfix.c: the GOT slots of absolute N64-address symbols in the PIE, put back at startup
 * (pc_mem_fix_absolute in memory.c). On its own so that test programs that place game data at its N64 addresses
 * can link it too (port/assets/tests/support.h). */
#define _GNU_SOURCE
#include <stdint.h>

int pc_fix_absolute_got(void);
void pc_relro_pages(uintptr_t start, uintptr_t size, uintptr_t page, uintptr_t *lo, uintptr_t *hi);

#ifdef _WIN32
/* PE has nothing to fix: code reaches every extern through a .refptr slot, which the linker fills with an
 * absolute symbol's value, and absolute symbols get no base relocations, so the slots keep it when the image
 * moves (docs/port/WINDOWS.md). */
int pc_fix_absolute_got(void) { return 0; }
#else
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>

/* The pages of a RELRO segment the loader made read-only: both ends rounded down, as glibc's _dl_protect_relro
 * does. A RELRO end inside a page leaves that page writable, with the start of .data in it (16 KiB or 64 KiB
 * pages, and binutils that align the end to 4 KiB): rounding up made that .data read-only, and the first write to
 * it faulted (a Pi 5 running an Ubuntu 22.04 build). Nothing to protect when hi <= lo. */
void pc_relro_pages(uintptr_t start, uintptr_t size, uintptr_t page, uintptr_t *lo, uintptr_t *hi)
{
    *lo = start & ~(page - 1);
    *hi = (start + size) & ~(page - 1);
}

/* gen_stubs puts data the game uses by its N64 address at that address (aliases.ld: the low-memory globals,
 * the heap start, cartridge offsets), and gen_data every arena symbol (data.c: about 1,800 names of the game's
 * data and BSS in the window). binutils 2.40 on aarch64 gives the GOT slot of such an absolute symbol a RELATIVE
 * relocation, so at run time it holds the load address plus the value. Nothing the linker places has a link-time
 * address at 0x80000000 or above (gen_data.py --check verifies it), so a RELATIVE relocation with an addend there
 * is one of these: put the value back. Initialized pointers to them (R_*_ABS64) are resolved at link time. Runs
 * before any game code, and before anything touches game data. */
static int fix_absolute(struct dl_phdr_info *info, size_t size, void *count)
{
    (void)size;
    if (info->dlpi_name && info->dlpi_name[0])
        return 0;   /* the executable comes first, with no name */
    uintptr_t base = info->dlpi_addr, relro = 0, relro_size = 0;
    const ElfW(Dyn) *dyn = NULL;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        if (ph->p_type == PT_DYNAMIC)
            dyn = (const ElfW(Dyn) *)(base + ph->p_vaddr);
        else if (ph->p_type == PT_GNU_RELRO)
            relro = base + ph->p_vaddr, relro_size = ph->p_memsz;
    }
    uintptr_t rela = 0;
    size_t relasz = 0;
    for (; dyn && dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_RELA)
            rela = dyn->d_un.d_ptr;
        else if (dyn->d_tag == DT_RELASZ)
            relasz = dyn->d_un.d_val;
    }
    if (!rela || !base)
        return 1;
    if (rela < base)
        rela += base;   /* the dynamic loader leaves some ports' entries unrelocated */
#if defined(__aarch64__)
    const unsigned relative = R_AARCH64_RELATIVE;
#elif defined(__x86_64__)
    const unsigned relative = R_X86_64_RELATIVE;
#else
    const unsigned relative = ~0u;
#endif
    uintptr_t lo, hi;
    pc_relro_pages(relro, relro_size, (uintptr_t)sysconf(_SC_PAGESIZE), &lo, &hi);
    int opened = 0;
    for (const ElfW(Rela) *r = (const ElfW(Rela) *)rela; (uintptr_t)(r + 1) <= rela + relasz; r++) {
        uint64_t v = (uint64_t)r->r_addend;
        if (ELF64_R_TYPE(r->r_info) != relative || v < 0x80000000u || v >= 0xC0000000u)
            continue;
        uint64_t *slot = (uint64_t *)(base + r->r_offset);
        if (*slot != base + v)
            continue;
        if (!opened && hi > lo && mprotect((void *)lo, hi - lo, PROT_READ | PROT_WRITE) == 0)
            opened = 1;
        *slot = v;
        ++*(int *)count;
    }
    if (opened)
        mprotect((void *)lo, hi - lo, PROT_READ);
    return 1;
}

/* The number of slots put back. */
int pc_fix_absolute_got(void)
{
    int count = 0;
    dl_iterate_phdr(fix_absolute, &count);
    return count;
}
#endif
