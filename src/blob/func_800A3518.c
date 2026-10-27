/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
#ifdef TARGET_PC
/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct { u8 pad0[12]; PcPfs pfs; s32 error, FreeSpace; u8 pad7C[16]; struct { s32 v; u8 pad[36]; } e[16]; } Car;
typedef struct { u8 pad0[12]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } CarSlot;
#else
typedef struct { u8 pad0[140]; struct { s32 v; u8 pad[36]; } e[16]; } Car;
typedef struct { u8 b[772]; } CarSlot;
#endif
extern CarSlot D_80144030[];
s32 func_800A3518(s32 i) {
    s32 n = 0;
    s32 k;
    Car *c = (Car *)&D_80144030[i];
    for (k = 0; k < 16; k++) {
        if (c->e[k].v == 0) {
            n++;
        }
    }
    return n;
}
