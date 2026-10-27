/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800F0F44 = GC _FindGhosts(pak) (highscorescr.c): controller-pak ghost status for the high-score screen.
 * Status D_801461C0[pak]: -4 no pak (D_80156CF0[pak].inserted), -1 unformatted, -2 damaged, -3 full (slot
 * bytes 1/5/6 of D_80144030[pak], read through the inlined helpers pak_slot_b1/b5/b6, which give retail's
 * 72-byte frame), else 0; in a ghost race (gRace D_8014A110 == 2) walk the ghost list D_80152028 and record
 * the matching node in D_80146198[pak]; 1 = same ghost as the player's, 2 = a different one.
 * draft (8/116); : the `= 2` arm first (retail block layout) plus ONE DEVICE:
 *  - compiled-out `do {} while (0);` right after `if (D_8014A110 == 2) {` (GC has a GhostFileAvailable()
 *    check at this point). It lengthens only the constant-2 web (born at the gRace compare), so its
 *    priority drops below the 1/&D_8014978C/&D_80152570 tie (save 2.0 -> below) and 2 takes s7 as in retail.
 *    Without it: 12 words (colour-only, force.sh proven); with `= 1` first instead: 9 rows (layout).
 * pak_slot_b1/b5/b6 are non-static internal helpers (no deleted stubs); force_internal them.
 */
typedef signed int s32;
typedef unsigned int u32;
typedef signed char s8;
typedef unsigned char u8;

#ifdef TARGET_PC
/* PC: the pak file record's and the profile's host layouts (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Q { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 id; u8 pad17;
    s8 name[14]; } Q;
#else
typedef struct Q { u8 pad[16]; u8 id; u8 pad17; s8 name[14]; } Q;
#endif
typedef struct Ghost { struct Ghost **next; Q **q; s8 track; u8 backwards; s8 pos[14]; s32 id0; s32 id1; } Ghost;
#ifdef TARGET_PC
typedef struct Player { void *w0, *w4; Q **qq; s32 id0; s32 id1; s8 name[4]; } Player;
#else
typedef struct Player { s32 w0; s32 w4; Q **qq; s32 id0; s32 id1; s8 name[4]; } Player;
#endif
#ifdef TARGET_PC
/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct Slot { u8 pad0; s8 formatted; u8 pad2[3]; s8 damaged; s8 full; u8 rest[5]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } Slot;
#else
typedef struct Slot { u8 pad0; s8 formatted; u8 pad2[3]; s8 damaged; s8 full; u8 rest[765]; } Slot;
#endif   /* 772 bytes */
typedef struct PakState { s8 inserted; u8 rest[15]; } PakState;

extern Player **D_8014A160;     /* gGhostPlayer */
extern Ghost **D_80146198[];    /* gPakGhosts */
extern s32 D_801461C0[];        /* gGhostStatus */
extern PakState gControllerData[];
extern Slot D_80144030[];
extern s32 D_8014A110;          /* gRace */
extern Ghost **D_80152028;      /* gGhostFiles list head */
extern s8 D_8014978C;           /* gTrackNum */
extern s8 D_80152570;           /* gBackwards */
extern s32 func_800950AC(void *, void *, s32);
extern s32 func_8008AD04(void *, void *);

s32 pak_slot_b1(s32 pak) { return D_80144030[pak].formatted; }
s32 pak_slot_b5(s32 pak) { return D_80144030[pak].damaged; }
s32 pak_slot_b6(s32 pak) { return D_80144030[pak].full; }

void FindGhosts(s32 pak)
{
    Player *player;
    Ghost ***ghosth;
    Ghost **node;
    Ghost *ghost;

    player = *D_8014A160;
    ghosth = &D_80146198[pak];
    *ghosth = 0;
    if (gControllerData[pak].inserted == 0) {
        D_801461C0[pak] = -4;
    } else if (pak_slot_b1(pak) == 0) {
        D_801461C0[pak] = -1;
    } else if (pak_slot_b5(pak) != 0) {
        D_801461C0[pak] = -2;
    } else if (pak_slot_b6(pak) != 0) {
        D_801461C0[pak] = -3;
    } else {
        D_801461C0[pak] = 0;
        if (D_8014A110 == 2) {
            do {} while (0);
            for (node = D_80152028; node != 0; node = ghost->next) {
                ghost = *node;
                if (ghost->q != 0 && (*ghost->q)->id == pak && ghost->track == D_8014978C &&
                    ghost->backwards == D_80152570 &&
                    func_800950AC((*ghost->q)->name, (*player->qq)->name, 14) == 0) {
                    *ghosth = node;
                    if (func_8008AD04(ghost->pos, player->name) != 0 || player->id0 != ghost->id0 ||
                        player->id1 != ghost->id1) {
                        D_801461C0[pak] = 2;
                    } else {
                        D_801461C0[pak] = 1;
                    }
                }
            }
        }
    }
}
