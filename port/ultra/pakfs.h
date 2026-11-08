/* port/ultra/pakfs.h: a controller pak's notes as files (docs/port/SAVES.md).
 *
 * pakfs.c: the formats, pure (no messages, no game code): a pak image's notes read out and laid into an image, the
 * save files that hold one note each (a profile, a ghost: .r49save, .r49ghost, .r49note), the game's own checks of a
 * profile and a ghost, and the names the files get.
 * pakstore.c: each player's saves folder (game.saves = files): the notes read at start, the pak the game sees built
 * from them, and what the game writes split back into files by the writer thread (pakwrite.c). */
#ifndef PC_PAKFS_H
#define PC_PAKFS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- pakfs.c: notes and save files */

#define PC_PAK_PAGE 256
#define PC_PAK_NOTES 16
#define PC_PAK_DATA_PAGES 123       /* pages 5-127 of a one-bank pak */
#define PC_NOTE_HEADER 128          /* a save file's PC header */
#define PC_RUSH_GAME_CODE 0x4E525545u   /* the game's note: game and company codes (group.c's PS_CreateFile) */
#define PC_RUSH_COMPANY 0x3544u
#define PC_GHOST_HEADER 88          /* GhostHeader, the N64's layout (docs/port/POINTERS.md 9.10) */
#define PC_GHOST_REVISION 17
#define PC_GHOST_TRACKS 6           /* the race tracks, 0-5: a ghost race's (the profile's six records each way) */
#define PC_GHOST_CARS 13            /* the car types (the profile's 13 car blocks, the car tables' [13]) */
#define PC_GHOST_MAX_FRAMES (60 * 60 * 20)   /* 20 minutes of samples at 60 a second: far past any race's limit */
#define PC_PROFILE_SIZE_N64 2064    /* tSaveData */

/* A note: its directory entry (32 bytes, the native Pfs C's: host-order game code, company code, data sum; its start
 * page zero here, since where its pages are is the image's), its place in the directory, and its pages. */
typedef struct {
    uint8_t dir[32];
    int slot;        /* 0-15; -1: none (the first free) */
    int pages;       /* 1-123 */
    uint8_t *data;   /* pages * 256 bytes, malloc'd */
} PcNote;

enum { PC_NOTE_OTHER = 0, PC_NOTE_PROFILE = 1, PC_NOTE_GHOST = 2 };
/* What a note holds, from its bytes: the game's (its codes) profile (the save data's first block's checksum, and its
 * revision byte 16) or ghost (revision 17 and the header's checksum), or anything else. */
typedef struct {
    int kind;
    int track, backward, car;   /* a ghost's; -1 unknown */
    char name[14];              /* the profile's name, the ghost's player: ASCII, as the game keeps it */
    float finish;               /* a ghost's race time, seconds; 0 unknown */
    uint32_t id[2];             /* the player's id words (the profile's +4, the ghost's playerId) */
} PcNoteInfo;

/* The save code's checksum (CalcChecksumInternal, src/blob/groups/CalcChecksum/group.c): the same bytes give the
 * same word as the game's. */
uint32_t pc_save_hash(const uint8_t *data, uint32_t size);
void pc_note_info(const PcNote *note, PcNoteInfo *info);
uint32_t pc_note_game_code(const PcNote *note);
uint16_t pc_note_company(const PcNote *note);
/* Two notes are one note on a pak: the same codes, extension and name (osPfsFindFile's key). */
int pc_note_same_key(const uint8_t *dir_a, const uint8_t *dir_b);
int pc_note_equal(const PcNote *a, const PcNote *b);   /* the same directory entry and pages */
int pc_note_copy(PcNote *to, const PcNote *from);      /* 0, or -1 without memory */
void pc_note_free(PcNote *note);

/* The notes of a pak image (PC_PAK_SIZE bytes, the native Pfs C's: si.c): each used directory entry whose pages
 * chain as osPfsFileState follows them (the inode table, or its backup when the first's checksum is wrong). A note
 * whose chain is broken (a page outside 5-127, a loop, no end) is left out and counted in *damaged. The number of
 * notes, in directory order; -1 when neither inode table is good (why says so). */
int pc_pak_notes(const uint8_t *image, PcNote out[PC_PAK_NOTES], int *damaged, char *why, size_t why_size);
/* A pak image of these notes: formatted (pakfile.c), its first page (the label and the ID block with its three
 * copies) page0's when it is given, then each note at its slot when that is free (else the first free one), its pages
 * one after another from page 5 in slot order, the inode table and its backup with their checksums. -1 when they don't
 * fit (17 or more notes, more than 123 pages; why says which). slots_out (16) gets the note index at each slot, -1. */
int pc_pak_build(uint8_t *image, const uint8_t *page0, const PcNote *const *notes, int n, int *slots_out, char *why,
                 size_t why_size);

/* A ghost's bytes (a note's pages, or a file's) as the game will take them: the header's revision and checksum, its
 * track (0-5), direction (0-1) and car (0-12), the zipped data inside the note and its checksum, and, when inflate is
 * set, the data inflated (raw deflate, as the game's unzipdata) to exactly the unzipped size, a whole number
 * of 3-byte frames up to PC_GHOST_MAX_FRAMES, with its checksum: what DecodeGhost and the replay read (an inflate past
 * the buffer would run on through the game's heap). 0, or -1 with why. */
int pc_ghost_check(const uint8_t *data, size_t size, int inflate, char *why, size_t why_size);

/* ---------------------------------------------------------------- save files */

/* A save file: a 128-byte PC header, then the note's pages as the game wrote them (the N64's layout; the game writes
 * its words natively, so they are little-endian, as on every host the port builds for; byte 0x0B says so). The
 * header's integers are little-endian whatever the host:
 *   0x00 "R49S"  0x04 u16 version 1  0x06 u16 header size 128  0x08 u8 kind (PC_NOTE_*)  0x09 u8 origin (PC_ORIGIN_*)
 *   0x0A u8 the note's directory slot (0xFF none)  0x0B 'L' the note's byte order  0x0C u32 the data's size
 *   0x10 u32 CRC-32 of the file with this field zero  0x14 the directory entry (32)  0x34 u8 track, 0x35 direction,
 *   0x36 car (0xFF unknown)  0x37 0  0x38 u32 a ghost's time in ms (0xFFFFFFFF unknown)  0x3C the name (16, NUL-padded)
 *   0x4C the port's version that wrote it (32, NUL-padded)  0x6C 0 (20)  0x80 the pages
 * The kind and the fields after the directory entry are from the note's bytes, for tools: a reader takes the note's
 * own (pc_note_info), and a header whose kind disagrees with the note's is a damaged file (the other fields are
 * covered by the CRC alone). */
enum { PC_ORIGIN_GAME = 0, PC_ORIGIN_IMPORT = 1, PC_ORIGIN_EXPORT = 2 };
typedef struct {
    int version, kind, origin, slot;
    char writer[33];
} PcSaveFileInfo;
/* The file's bytes, malloc'd, into *out (*size). 0, or -1 without memory. */
int pc_save_file_make(const PcNote *note, int origin, const char *writer, uint8_t **out, size_t *size);
/* A file's bytes: the header, its size fields, the CRC, the byte order, the pages (1-123), the kind against the
 * note's own. The note (its data malloc'd) and the header's facts. 0, or -1 with why ("its CRC is wrong", ...). */
int pc_save_file_read(const uint8_t *buf, size_t size, PcNote *note, PcSaveFileInfo *info, char *why,
                      size_t why_size);
/* A name for the note's file, readable and safe on Windows and Linux (no \ / : * ? " < > |, no control characters,
 * no trailing dot or space, not CON, NUL...): "ALICE.r49save", "track 4 backward - ALICE - 1m01.50s.r49ghost",
 * "note 4E525545 2.r49note". Without the folder. */
void pc_save_file_name(const PcNote *note, char *out, size_t size);
/* A file name's characters made safe as above (in place). */
void pc_save_safe_name(char *name);

/* ---------------------------------------------------------------- pakstore.c: each player's saves folder */

/* The folder (game.saves_dir: saves/p1 in the data folder): the notes' files, and
 *   pak-id.bin   the pak's first page (its label and ID), so the game sees one pak from run to run
 *   pak.txt      the notes on the pak the game had last ("slot<TAB>file" lines)
 *   pins.txt     the ghosts the player pinned to the pak, one file a line
 *   imported.txt the record of the .mpk imported at the first start with files (it is imported once)
 *   import/      .r49ghost files dropped in, taken at start and by the F1 Saves page (then import/done/, or
 *                import/refused/ with the reason in the log)
 *   export/      ghosts exported from the Saves page
 *   trash/       every file deleted, by the game or the player: nothing is removed
 *   backup/      each note file's last three versions that started a run (NAME.bak, .bak2, .bak3)
 * Thread-safe: the main thread opens, builds and changes it; the writer thread splits images into it. */
#define PC_STORE_NAME 192
typedef struct {
    char file[PC_STORE_NAME];   /* in the folder */
    char label[64];             /* "T1 fwd ALICE 1:02.35", "ALICE" */
    int kind;                   /* PC_NOTE_* */
    int track, backward, car;
    char name[14];
    float finish;
    int pages;
    int on_pak;                 /* on the pak the game has now */
    int pinned, imported, deleting;   /* deleting: deleted from F1 while on the pak; it goes at the next swap */
} PcStoreEntry;

/* Before the game starts, for controller ch (0-3) with game.saves = files: the folder made, its files read (backups
 * made, damaged ones set aside and restored from a backup), the .mpk at mpk imported the first time (never changed),
 * the import folder taken in. writer is the port's version, written into each file. 0, or -1 (msg says why) when the
 * folder can't be made: then no pak for that controller. msg gets a summary for the log either way. */
int pc_store_open(int ch, const char *dir, const char *mpk, const char *writer, char *msg, size_t msg_size);
/* The same, done when the pak is first built (pc_store_build): a controller whose pak is chosen later. */
void pc_store_configure(int ch, const char *dir, const char *mpk, const char *writer);
void pc_store_close(int ch);                 /* tests */
int pc_store_is_open(int ch);
const char *pc_store_dir(int ch);            /* the folder, once opened or configured; else NULL */
/* The pak the game is to see (PC_PAK_SIZE bytes): the profiles and other games' notes; the pinned ghosts; the ghosts
 * on the pak last time; then the fastest ghost of each player (an import apart from the player's own) for each track
 * and direction while 1 note and 24 pages stay free. An imported ghost gets a name of the PC's on the pak (its name's last two characters digits, which
 * the game's own ghost names never end in) and the game's ghost name prefix and extension when they are known
 * (pc_store_set_game_names). 0, or -1 with why. */
int pc_store_build(int ch, uint8_t *image, char *why, size_t why_size);
/* The writer thread's write for a folder (a PcPakWriteFn: path is the folder): the image's notes against the files
 * of what the game had: a note the game added is a new file, one it changed in place (a profile) its file written
 * again, one it deleted, or deleted and made again (a ghost: SaveGhost), moves to trash/ (the new one a file of its
 * own); a ghost or profile not whole yet (a write in progress) waits for the next image. Each file written
 * atomically. -1 with errno when a file can't be written (the writer keeps the image and tries again). */
int pc_store_write(const char *dir, const void *image, size_t size);
int pc_store_list(int ch, PcStoreEntry *out, int max);   /* profiles first, then ghosts by track, way and time */
/* The F1 Saves page's actions: 0 done, -1 not (msg says why, in a line for the footer). A change of what is on the
 * pak (a pin, an unpin, an import, a delete of a note on it) waits for pc_store_swap_wanted. */
int pc_store_pin(int ch, const char *file, int on, char *msg, size_t msg_size);
int pc_store_delete(int ch, const char *file, char *msg, size_t msg_size);
int pc_store_export_ghost(int ch, const char *file, char *msg, size_t msg_size);
int pc_store_import(int ch, char *msg, size_t msg_size);                       /* the import folder */
/* What the import folder gave at start ("Imported ...", "Refused X: why", counts), for the Saves page; 0: nothing. */
int pc_store_notice(int ch, char *out, size_t size);
int pc_store_import_file(int ch, const char *path, char *msg, size_t msg_size);
int pc_store_export_mpk(int ch, const char *mpk, char *msg, size_t msg_size);  /* the old one kept as .bak */
/* What is on the pak the game has differs from what it should have: si.c swaps the pak when that is safe. */
int pc_store_swap_wanted(int ch);
/* The game's own note name prefix (8 characters) and ghost extension (4), in the pak's character set, from the
 * game's data (main): an imported ghost is given them on the pak, so the game reads it as a ghost. */
void pc_store_set_game_names(const uint8_t prefix[8], const uint8_t ghost_ext[4]);

#ifdef __cplusplus
}
#endif

#endif
