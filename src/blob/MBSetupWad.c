/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Binds a table header and rebases its 12-byte entries; no quirks beyond storing through table->header. */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

typedef struct Entry {
    s32 unk0;
    s32 offset;
    s32 unk8;
} Entry;

typedef struct Header {
    u32 entries;
    s32 count;
} Header;

typedef struct Table {
    Header *header;
    s32 count;
    Entry *entries;
} Table;

/* A WAD's table rebased in place: its entries' 32-bit N64 addresses are the file's offsets plus the block's N64
 * address, N64A(p) a window pointer's and N64PTR(T, a) the pointer at one (pc_prelude.h, docs/port/NOFIXED.md 2.3).
 * Before the rebase the table's first entry address is an offset, which stays as it is. */
#define N64A(x) PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))
#define WAD_ENTRIES(a) ((a) & 0x80000000 ? N64PTR(Entry *, a) : (Entry *)(uintptr_t)(a))
void MBSetupWad(Table *table, Header *header) {
    s32 i;
    Entry *e;

    if (table != 0) {
        table->header = header;
        table->entries = WAD_ENTRIES(header->entries);
        table->count = header->count;
        if (!(header->entries & 0x80000000)) {
            table->entries = N64PTR(Entry *, ((u32) table->entries + N64A(header)));
            table->header->entries = N64A(table->entries);
            for (i = 0; i < table->count; i++) {
                e = &table->entries[i];
                e->offset += N64A(header);
            }
        }
    }
}
