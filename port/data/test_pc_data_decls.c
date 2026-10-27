/* port/data/test_pc_data_decls.c: declarations for pc_data_test that go through gen_data.py's layout rules.
 *
 * gen_data.py --test-tables reads this object's DWARF as it reads the game's and writes the runs and the element
 * conflicts it finds for them (pc-data/test_pc_data_gen.h); test_pc_data.c loads them from its synthetic game image
 * and checks the bytes. Never linked: the pointer table only makes the compiler keep the declarations in the
 * DWARF. Each name is the N64 address in the test's game image (0x80010000-0x80010200), before its arena. */
#include <stdint.h>

/* A colour union as the game declares them (TextColor, tRGBA): R, G, B, A in memory, the word only moves them.
 * A byte member covers it, so it is copied as bytes. */
typedef union { uint8_t c[4]; uint32_t rgba; } TestColor;
/* The other spelling (the runtime images' Color). */
typedef union { uint32_t word; struct { uint8_t r, g, b, a; } rgba; } TestColor2;
/* No byte member covers it: its word is swapped. */
typedef union { uint32_t bits; float value; } TestWord;
/* The byte member is shorter than the union, so the widest member sets the layout (MenuSlot). */
typedef union { struct { int16_t x, y; uint8_t flags[4]; } v; uint8_t kind[4]; } TestPart;

extern TestColor D_80010020[2];
extern uint32_t D_80010024;          /* a word view of element 1 (D_801146C0 in D_801146BC): its bytes stay */
extern TestColor2 D_80010028;
extern TestWord D_8001002C;
extern TestPart D_80010030;
/* Two interleaved columns of an s16[3][4] declared to the last element read (gViewportOffsetX/Y): they agree
 * where they overlap. Then a flag. */
extern int16_t D_80010040[9], D_80010042[9];
extern int8_t D_80010054;
/* A table declared too long (gViewportOffsetX as s16[32]): it runs over the flag after it, which is listed. */
extern int16_t D_80010060[32];
extern int8_t D_80010070;

void *const pc_test_decls[] = {
    D_80010020, &D_80010024, &D_80010028, &D_8001002C, &D_80010030, D_80010040, D_80010042, &D_80010054,
    D_80010060, &D_80010070,
};
