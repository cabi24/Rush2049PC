/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800F0F44 = GC _FindGhosts(pak) (highscorescr.c): controller-pak ghost status for the high-score screen.
 * Status gGhostStatus[pak]: -4 no pak (gControllerData[pak].inserted), -1 unformatted, -2 damaged, -3 full (slot
 * bytes 1/5/6 of gPaks[pak], read through the inlined helpers pak_slot_b1/b5/b6, which give retail's
 * 72-byte frame), else 0; in a ghost race (gRace gRace == 2) walk the ghost list gGhostFiles_head and record
 * the matching node in gPakGhosts[pak]; 1 = same ghost as the player's, 2 = a different one.
 * draft (8/116); : the `= 2` arm first (retail block layout) plus ONE DEVICE:
 *  - compiled-out `do {} while (0);` right after `if (gRace == 2) {` (GC has a GhostFileAvailable()
 *    check at this point). It lengthens only the constant-2 web (born at the gRace compare), so its
 *    priority drops below the 1/&gTrackNum/&gBackwards tie (save 2.0 -> below) and 2 takes s7 as in retail.
 *    Without it: 12 words (colour-only, force.sh proven); with `= 1` first instead: 9 rows (layout).
 * pak_slot_b1/b5/b6 are non-static internal helpers (no deleted stubs); force_internal them.
 */
typedef signed int s32;
typedef unsigned int u32;
typedef signed char s8;
typedef unsigned char u8;

/* PC: the pak file record's and the profile's host layouts (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Q { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 id; u8 pad17;
    s8 name[14]; } Q;
typedef struct Ghost { struct Ghost **next; Q **q; s8 track; u8 backwards; s8 pos[14]; s32 id0; s32 id1; } Ghost;
typedef struct Player { void *w0, *w4; Q **qq; s32 id0; s32 id1; s8 name[4]; } Player;
/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct Slot { u8 pad0; s8 formatted; u8 pad2[3]; s8 damaged; s8 full; u8 rest[5]; PcPfs pfs; u8 pad74[0x304 - 0x74]; } Slot;
typedef struct PakState { s8 inserted; u8 rest[15]; } PakState;

extern Player **gPlayers_save;     /* gGhostPlayer */
extern Ghost **gPakGhosts[];    /* gPakGhosts */
extern s32 gGhostStatus[];        /* gGhostStatus */
extern PakState gControllerData[];
extern Slot gPaks[];
extern s32 gRace;          /* gRace */
extern Ghost **gGhostFiles_head;      /* gGhostFiles list head */
extern s8 gTrackNum;           /* gTrackNum */
extern s8 gBackwards;           /* gBackwards */
extern s32 game_strncmp(void *, void *, s32);
extern s32 game_strcmp(void *, void *);

s32 pak_slot_b1(s32 pak) { return gPaks[pak].formatted; }
s32 pak_slot_b5(s32 pak) { return gPaks[pak].damaged; }
s32 pak_slot_b6(s32 pak) { return gPaks[pak].full; }

void FindGhosts(s32 pak)
{
    Player *player;
    Ghost ***ghosth;
    Ghost **node;
    Ghost *ghost;

    player = *gPlayers_save;
    ghosth = &gPakGhosts[pak];
    *ghosth = 0;
    if (gControllerData[pak].inserted == 0) {
        gGhostStatus[pak] = -4;
    } else if (pak_slot_b1(pak) == 0) {
        gGhostStatus[pak] = -1;
    } else if (pak_slot_b5(pak) != 0) {
        gGhostStatus[pak] = -2;
    } else if (pak_slot_b6(pak) != 0) {
        gGhostStatus[pak] = -3;
    } else {
        gGhostStatus[pak] = 0;
        if (gRace == 2) {
            do {} while (0);
            for (node = gGhostFiles_head; node != 0; node = ghost->next) {
                ghost = *node;
                if (ghost->q != 0 && (*ghost->q)->id == pak && ghost->track == gTrackNum &&
                    ghost->backwards == gBackwards &&
                    game_strncmp((*ghost->q)->name, (*player->qq)->name, 14) == 0) {
                    *ghosth = node;
                    if (game_strcmp(ghost->pos, player->name) != 0 || player->id0 != ghost->id0 ||
                        player->id1 != ghost->id1) {
                        gGhostStatus[pak] = 2;
                    } else {
                        gGhostStatus[pak] = 1;
                    }
                }
            }
        }
    }
}
