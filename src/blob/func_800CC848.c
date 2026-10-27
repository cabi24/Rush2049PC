/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned char u8;
typedef signed char s8;
typedef int s32;
#ifdef TARGET_PC
/* PC: the pak file record (Owner, car is its pak) and the profile (Object, node is its file handle) on their host
 * layouts (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Owner { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 car; } Owner;
typedef struct Node { Owner *owner; } Node;
typedef struct Object { void *next, *prev; Node *node; } Object;
#else
typedef struct Owner { u8 pad[16]; u8 car; } Owner;
typedef struct Node { Owner *owner; } Node;
typedef struct Object { u8 pad[8]; Node *node; } Object;
#endif
#ifdef TARGET_PC
/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct Car { u8 pad0; s8 enabled; u8 rest[10]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } Car;
#else
typedef struct Car { u8 pad0; s8 enabled; u8 rest[770]; } Car;
#endif
extern Car D_80144030[];
s32 func_800A1A60(Node *);
s32 func_800CC848(Object **object, s32 action)
{
    Node *node = (*object)->node;
    s32 car;
    s32 status = 1;
    if (!node) return status;
    car = node->owner->car;
    status = D_80144030[car].enabled;
    if (!status) return 0;
    if (!action) return 1;
    return func_800A1A60(node);
}

/*
 * Complete 128-byte native body at 0x800CC848. A missing nested node succeeds;
 * a disabled indexed record fails; an inactive action succeeds; otherwise
 * delegate to the real func_800A1A60 and preserve its return value.
 * Owner/Node/Object/Car are minimal offset views, not recovered original types.
 * The 772-byte table stride and signed enabled byte follow the native loads.
 * No bounds checking or null validation beyond the native node test is added.
 * Source-only strict match; image splicing and full-ROM verification remain.
 */
