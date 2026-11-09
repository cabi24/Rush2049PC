/* pakscr_FindFile: the controller-pak manager's file lookup (MAT3 pakscr.c FindFile). */
#include "game/game.h"

/* Each pak's open files (paksysdc's tDLList, MAT3 tDLList): head the first. */
typedef struct tDLList {
    u8 indirect, doubly, opaque2[2];
    unsigned int count;
    void **head, **tail;
} tDLList;
VIEW(tDLList[], gPS_Files);
#define gPS_Files gPS_Files_view

/* A pak file record (MAT3 tPS_File, paksysdc) in its PC layout, 96 bytes: its links and its two functions are
 * pointers, so the rest is later than on the N64 (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10). */
typedef struct PakFile {
    struct PakFile *next, *prev;
    void (*RemoveFile)(void *), (*MoveFile)(void *, void *);
    u8 pak;                 /* 0x20 (N64 0x10) */
    u8 fileNo;              /* 0x21 (N64 0x11) its number among the pak's notes */
} PakFile;

/* The list node of the chosen pak's (gPickedPak) file number id, NULL if it has none. */
void **pakscr_FindFile(s32 id)
{
    void **node = gPS_Files[gPickedPak].head;

    while (node != 0) {
        PakFile *file = *node;
        if (id == file->fileNo) {
            break;
        }
        node = *(void ***)file;
    }
    return node;
}
