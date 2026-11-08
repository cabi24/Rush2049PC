/*
 * Overlay loader (N64-only; no arcade ancestor).  Historical labels: load_overlay (0x800A11E4),
 * load_extra_overlay (0x800A1244), load_start_overlay (0x800A133C).  allocate_memory_at is the game
 * heap's allocate-at-address (alloc_at.c in this group).
 *
 * load_overlay = load_overlay(vram, name, vramEnd, bssEnd, romStart, bssStart, &loaded, romData):
 * if the overlay is not loaded, reserve [vram, vramEnd) in the game heap, mark the block
 * (0x800962D4), inflate the compressed image to vram, clear its bss and set the flag.
 * load_extra_overlay loads overlay "Extra" (flag extra_overlay_loaded), load_start_overlay loads "Start"
 * (flag start_overlay_loaded).  LoadFrontEndOverlay (0x800C8FA4, unmatched, internal; real caller
 * updateGameState unmatched) unloads "Extra", inlines the "Start" load and unloads "Start".
 *
 * load_overlay is internal (IPA register parameters: s0 vram, a2 vramEnd, s1 bssEnd, s2 bssStart,
 * s3 &loaded, s4 romData; name and romStart are stack-passed and unused).
 * Shaping quirks (compile-affecting, may not be original source):
 *  - `if (&name == 0) return;` is code-free; it homes `name` in memory (callers store it at
 *    4(sp)) and keeps umerge from inlining load_overlay into its three callers.
 *  - the four code-free `if (x) {}` reads set the callee-saved colouring order of the register
 *    parameters (uopt orders webs by accumulated read count; workbench lever 9).  Without them the
 *    order is s0 vram, s1 loaded, s2 bssStart, s3 romData, s4 bssEnd.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;

extern s8 start_overlay_loaded;
extern s8 extra_overlay_loaded;
extern u8 *start_overlay_rom;
extern u8 *extra_overlay_rom;
/* Each runtime image's BSS, [start, end): image B's ("Extra") and image A's ("Start"). The starts are the addresses of
 * the images' first BSS objects (gShotObjList, gTrophyMat), the symbol tables' names for the bounds. */
extern u8 extra_overlay_bss_start[], extra_overlay_bss_end[];
extern u8 start_overlay_bss_start[], start_overlay_bss_end[];
extern u8 D_00BE4C70[];
extern u8 D_00BDA100[];

void *allocate_memory_at(u32 addr, u32 size);
/* The heap's API takes 32-bit N64 addresses (PurgeableMemory's definition a u32): N64A(p) is a window pointer's,
 * N64A0(p) the same where the N64 passes the pointer itself, and N64PTR(T, a) the pointer at N64 address a, the
 * images' place in the heap (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
void PurgeableMemory(u32 ptr, s32 flag);
#define N64A(x) PC_N64A(x)
#define N64A0(x) PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))
void inflate_decompress(u8 *src, u8 *dst, s32 flag);
void bzero(void *p, u32 n);
/* PC (docs/port/DATA.md): both runtime images are linked in, and a load resets the image's data to its ROM
 * values and zeroes its BSS: 0 = image A ("Start", flag start_overlay_loaded), 1 = image B ("Extra", flag
 * extra_overlay_loaded). The game-heap reservation at the image address stays. */
void pc_data_overlay_load(int overlay);
/* No ROM addresses on the PC: the images come from port/data. */
#define D_00BE4C70 0
#define D_00BDA100 0
#define start_overlay_rom 0
#define extra_overlay_rom 0

void load_overlay(u8 *dst, char *name, u8 *end, u8 *bssEnd, u8 *romStart, u8 *bssStart, s8 *loaded, u8 *rom) {
    void *p;

    if (*loaded != 0) {
        return;
    }
    if (&name == 0) {
        return;
    }
    if (dst) {}
    if (bssEnd) {}
    if (bssEnd) {}
    if (bssStart) {}
    p = allocate_memory_at(N64A(dst), end - dst);
    PurgeableMemory(N64A0(p), 0);
    pc_data_overlay_load(loaded == &extra_overlay_loaded);
    *loaded = 1;
}

void load_extra_overlay(void) {
    load_overlay(N64PTR(u8 *, 0x8038A400), "Extra", N64PTR(u8 *, 0x803BB380), extra_overlay_bss_end, D_00BE4C70, extra_overlay_bss_start, &extra_overlay_loaded, extra_overlay_rom);
}

void load_start_overlay(void) {
    load_overlay(N64PTR(u8 *, 0x8038A400), "Start", N64PTR(u8 *, 0x803BB380), start_overlay_bss_end, D_00BDA100, start_overlay_bss_start, &start_overlay_loaded, start_overlay_rom);
}
