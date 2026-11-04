/* tools/port/test_ghost_c.c: the C's own verdicts, for tools/port/test_ghost.py to hold ghost.py against.
 *
 *   test_ghost_c check FILE...             each file as pc_save_file_read reads it, then a ghost as pc_ghost_check
 *                                          checks it with its samples inflated: "ok" or the C's reason, a line each
 *   test_ghost_c mpk FOLDER OUT [CH [PREFIX EXT]]
 *                                          the pak the game would be given from a saves folder (pc_store_open, then
 *                                          pc_store_export_mpk) written to OUT; CH the controller, 0-3; PREFIX and EXT
 *                                          the game's names in hex (pc_store_set_game_names). pc_store_open changes the
 *                                          folder (backups, import/): give it a copy.
 *   test_ghost_c import FOLDER FILE...     each file imported into a saves folder (pc_store_open, then
 *                                          pc_store_import_file, as the Saves page's import): its message, a line each
 *
 * Build (from the source tree): cc -std=gnu11 -Iport/ultra -Iport/include tools/port/test_ghost_c.c port/ultra/pakfs.c
 * port/ultra/pakstore.c port/ultra/pakfile.c -lz -lpthread -lm. Exit status: the number of files refused (check,
 * import), or 0 / 1 (mpk). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pakfs.h"
#include "ultra.h"

static int hex_bytes(const char *s, uint8_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        unsigned v;
        if (sscanf(s + 2 * i, "%2x", &v) != 1)
            return -1;
        out[i] = (uint8_t)v;
    }
    return strlen(s) == (size_t)(2 * n) ? 0 : -1;
}

int main(int argc, char **argv)
{
    char why[300];
    if (argc >= 3 && !strcmp(argv[1], "check")) {
        int refused = 0;
        for (int i = 2; i < argc; i++) {
            static uint8_t buf[1 << 20];
            FILE *f = fopen(argv[i], "rb");
            size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
            PcNote note;
            PcSaveFileInfo fi;
            int r = -1;
            if (!f)
                snprintf(why, sizeof why, "it can't be read");
            else if (pc_save_file_read(buf, n, &note, &fi, why, sizeof why) == 0) {
                r = fi.kind == PC_NOTE_GHOST ? pc_ghost_check(note.data, (size_t)note.pages * PC_PAK_PAGE, 1, why,
                                                              sizeof why) : 0;
                pc_note_free(&note);
            }
            if (f)
                fclose(f);
            printf("%s\t%s\n", argv[i], r == 0 ? "ok" : why);
            refused += r != 0;
        }
        return refused;
    }
    if (argc >= 4 && !strcmp(argv[1], "mpk")) {
        char msg[400];
        int ch = argc > 4 ? atoi(argv[4]) : 0;
        uint8_t prefix[8], ext[4];
        if (argc > 6) {
            if (hex_bytes(argv[5], prefix, 8) || hex_bytes(argv[6], ext, 4)) {
                fprintf(stderr, "PREFIX: 16 hex digits, EXT: 8\n");
                return 1;
            }
            pc_store_set_game_names(prefix, ext);
        }
        if (pc_store_open(ch, argv[2], NULL, "c", msg, sizeof msg) != 0) {
            fprintf(stderr, "%s\n", msg);
            return 1;
        }
        int r = pc_store_export_mpk(ch, argv[3], msg, sizeof msg);
        if (r != 0)
            fprintf(stderr, "%s\n", msg);
        pc_store_close(ch);
        return r != 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "import")) {
        char msg[400];
        int refused = 0;
        if (pc_store_open(0, argv[2], NULL, "c", msg, sizeof msg) != 0) {
            fprintf(stderr, "%s\n", msg);
            return 1;
        }
        for (int i = 3; i < argc; i++) {
            int r = pc_store_import_file(0, argv[i], msg, sizeof msg);
            printf("%s\t%s\n", argv[i], msg);
            refused += r != 0;
        }
        pc_store_close(0);
        return refused;
    }
    fprintf(stderr, "usage: %s check FILE... | mpk FOLDER OUT [CH [PREFIX EXT]] | import FOLDER FILE...\n", argv[0]);
    return 1;
}
