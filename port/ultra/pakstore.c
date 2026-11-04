/* port/ultra/pakstore.c: each player's saves as a folder of files (game.saves = files; docs/port/SAVES.md).
 *
 * The game's own pak code (paksys, the save code, the Pfs C) is left as it is: it sees a controller pak, as with
 * game.saves = pak, built here from the folder's files when the pak goes in (pc_store_build, si.c), and what it writes
 * there comes back here (pc_store_write, on the writer thread: pakwrite.c), each note it added, changed or deleted
 * a file written, written again or moved to trash/. So a profile or a ghost is a file of its own, any number of them
 * are kept, and a ghost can go to another player or another machine (.r49ghost). The pak the game sees holds what
 * its own structures hold (16 notes, 123 pages): the profiles, the pinned ghosts, the ghosts it had last time, then
 * each player's fastest ghost per track and way while room is left; a change of that (a pin, an import, a delete
 * from F1) is made as a hot swap when the game is at its title or main menu (si.c, main's check), else at the next
 * start. Every file is written atomically (pakfile.c's pc_file_write_atomic); nothing is ever removed: what the game
 * or the player deletes goes to trash/. Messages go to stderr (the log). */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include "pakfs.h"
#include "ultra.h"
#include "pc_compat.h"

#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif
#define PATHN 1100
#define RESERVE_NOTES 1     /* left free by the fastest-ghost step, for the game's next save */
#define RESERVE_PAGES 24
#define BACKUPS 3
#define MAX_FILE (PC_NOTE_HEADER + PC_PAK_DATA_PAGES * PC_PAK_PAGE)

typedef struct {
    char file[PC_STORE_NAME];
    PcNote note;           /* as its file holds it */
    PcNoteInfo info;
    int origin;            /* PC_ORIGIN_* */
    int pin;               /* its place among the pins + 1; 0: not pinned */
    int last;              /* its slot on the pak the game had last + 1; 0: not on it */
    int slot;              /* on the pak the game has: 0-15, -1 */
    int deleting;          /* deleted from F1 while on the pak: to the trash at the next build */
    int gone;              /* moved to the trash in this split: dropped at its end */
    uint8_t placed[32];    /* its directory entry as it is on the pak the game has */
} Entry;

typedef struct {
    int open;
    int configured;        /* pc_store_configure: opened when its pak is first built */
    char mpk[PATHN];       /* the .mpk to import the first time */
    char dir[PATHN];
    char label[40];        /* "controller 1's saves" */
    char writer[33];
    int ch;
    Entry *e;
    int n, cap;
    uint8_t page0[PC_PAK_PAGE];
    int have_page0;
    int on[PC_PAK_NOTES];  /* the entry at each slot of the pak the game has, -1 */
    int built;             /* the game has a pak from here */
    int swap;              /* it should have another */
    int failing;           /* a split's write failed: said once until one succeeds */
    char notice[240];      /* the import folder's last result at start, for the Saves page */
} Store;

static Store g_st[4];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_prefix[8], g_gext[4];
static int g_names;

/* ---------------------------------------------------------------- files */

static void join(char *out, size_t n, const char *a, const char *b)
{
    snprintf(out, n, "%s%s%s", a, a[0] && a[strlen(a) - 1] != '/' && a[strlen(a) - 1] != '\\' ? SEP : "", b);
}

static int exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static int is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int mkdirs(const char *dir)
{
    char p[PATHN];
    if (!dir[0])   /* the walk starts at its second byte */
        return (errno = ENOENT), -1;
    snprintf(p, sizeof p, "%s", dir);
    for (char *c = p + 1;; c++)
        if (*c == '/' || *c == '\\' || !*c) {
            char sep = *c;
            *c = 0;
            if (c[-1] != ':' && !is_dir(p) && mkdir(p, 0755) != 0 && errno != EEXIST)
                return -1;
            if (!sep)
                return is_dir(dir) ? 0 : -1;
            *c = sep;
        }
}

/* A whole file of at most cap bytes (one more is read, so a bigger file is told by *size > cap). */
static int read_file(const char *path, uint8_t **out, size_t *size, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    uint8_t *b = malloc(cap + 1);
    if (!b) {
        fclose(f);
        errno = ENOMEM;
        return -1;
    }
    size_t n = fread(b, 1, cap + 1, f);
    int bad = ferror(f);
    fclose(f);
    if (bad) {
        free(b);
        errno = EIO;
        return -1;
    }
    *out = b;
    *size = n;
    return 0;
}

static void local_time(const char *format, char *out, size_t n)
{
    time_t t = time(NULL);
    struct tm tm;
#ifdef _WIN32
    tm = *localtime(&t);
#else
    localtime_r(&t, &tm);
#endif
    strftime(out, n, format, &tm);
}

static int has_ext(const char *name, const char *ext)
{
    size_t a = strlen(name), b = strlen(ext);
    return a > b && !strcasecmp(name + a - b, ext);
}

static int note_file(const char *name)
{
    return has_ext(name, ".r49save") || has_ext(name, ".r49ghost") || has_ext(name, ".r49note");
}

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* The files of a folder with one of the note extensions, sorted; malloc'd names. ghosts: the import folder's (a
 * profile or another note there is read too, and refused with its reason). */
static int list_dir(const char *dir, int ghosts, char ***out)
{
    DIR *d = opendir(dir);
    char **v = NULL;
    int n = 0, cap = 0;
    *out = NULL;
    if (!d)
        return 0;
    for (struct dirent *de; (de = readdir(d));) {
        char path[PATHN];
        if (de->d_name[0] == '.' || !(ghosts ? has_ext(de->d_name, ".r49ghost") || has_ext(de->d_name, ".r49save") ||
                                                   has_ext(de->d_name, ".r49note")
                                             : note_file(de->d_name)))
            continue;
        join(path, sizeof path, dir, de->d_name);
        if (is_dir(path))
            continue;
        if (n == cap) {
            char **w = realloc(v, (size_t)(cap = cap ? cap * 2 : 16) * sizeof *v);
            if (!w)
                break;
            v = w;
        }
        v[n++] = strdup(de->d_name);
    }
    closedir(d);
    if (n)
        qsort(v, (size_t)n, sizeof *v, by_name);
    *out = v;
    return n;
}

static void free_list(char **v, int n)
{
    for (int i = 0; i < n; i++)
        free(v[i]);
    free(v);
}

/* path moved into folder under its own name, or "NAME (2).ext"... when taken; never over a file. */
static int move_into(const char *path, const char *folder, const char *name, char *moved, size_t n)
{
    char to[PATHN], base[PC_STORE_NAME], ext[32];
    const char *dot = strrchr(name, '.');
    snprintf(base, sizeof base, "%.*s", dot ? (int)(dot - name) : (int)strlen(name), name);
    snprintf(ext, sizeof ext, "%s", dot ? dot : "");
    if (mkdirs(folder) != 0)
        return -1;
    for (int k = 1; k < 10000; k++) {
        char want[PC_STORE_NAME + 48];
        if (k == 1)
            snprintf(want, sizeof want, "%s%s", base, ext);
        else
            snprintf(want, sizeof want, "%.150s (%d)%s", base, k, ext);
        join(to, sizeof to, folder, want);
        if (exists(to))
            continue;
        if (rename(path, to) != 0)
            return -1;
        if (moved)
            snprintf(moved, n, "%s", to);
        return 0;
    }
    errno = EEXIST;
    return -1;
}

/* ---------------------------------------------------------------- entries */

static Store *store_of(int ch)
{
    return ch >= 0 && ch < 4 && g_st[ch].open ? &g_st[ch] : NULL;
}

static int find_entry(Store *st, const char *file)
{
    for (int i = 0; i < st->n; i++)
        if (!st->e[i].gone && !strcmp(st->e[i].file, file))
            return i;
    return -1;
}

/* A file name of the note's own that no entry and no file in the folder has. */
static void unique_file(Store *st, const PcNote *note, char *out, size_t n)
{
    char want[PC_STORE_NAME], base[PC_STORE_NAME], path[PATHN];
    pc_save_file_name(note, want, sizeof want);
    const char *dot = strrchr(want, '.');
    snprintf(base, sizeof base, "%.*s", (int)(dot - want), want);
    for (int k = 1;; k++) {
        if (k == 1)
            snprintf(out, n, "%s", want);
        else
            snprintf(out, n, "%.150s (%d)%s", base, k, dot);
        join(path, sizeof path, st->dir, out);
        if (find_entry(st, out) < 0 && !exists(path))
            return;
    }
}

static int write_note(Store *st, const char *file, const PcNote *note, int origin)
{
    char path[PATHN];
    uint8_t *b;
    size_t size;
    if (pc_save_file_make(note, origin, st->writer, &b, &size) != 0) {
        errno = ENOMEM;
        return -1;
    }
    join(path, sizeof path, st->dir, file);
    int r = pc_file_write_atomic(path, b, size);
    int err = errno;
    free(b);
    errno = err;
    return r;
}

/* A new entry (the note copied), its file not written; its index, or -1. */
static int add_entry(Store *st, const PcNote *note, int origin, const char *file)
{
    if (st->n == st->cap) {
        Entry *e = realloc(st->e, (size_t)(st->cap = st->cap ? st->cap * 2 : 32) * sizeof *e);
        if (!e)
            return -1;
        st->e = e;
    }
    Entry *e = &st->e[st->n];
    memset(e, 0, sizeof *e);
    if (pc_note_copy(&e->note, note) != 0)
        return -1;
    snprintf(e->file, sizeof e->file, "%s", file);
    pc_note_info(&e->note, &e->info);
    e->origin = origin;
    e->slot = -1;
    memcpy(e->placed, e->note.dir, 32);
    return st->n++;
}

/* The entries moved to the trash dropped; the slots follow the entries' own (each entry on the pak knows its slot). */
static void drop_gone(Store *st)
{
    int j = 0;
    for (int i = 0; i < st->n; i++) {
        if (st->e[i].gone) {
            pc_note_free(&st->e[i].note);
            continue;
        }
        if (i != j)
            st->e[j] = st->e[i];
        j++;
    }
    st->n = j;
    for (int s = 0; s < PC_PAK_NOTES; s++)
        st->on[s] = -1;
    for (int i = 0; i < st->n; i++)
        if (st->e[i].slot >= 0 && st->e[i].slot < PC_PAK_NOTES)
            st->on[st->e[i].slot] = i;
}

/* A growing text (Windows' C runtime has no open_memstream). */
typedef struct {
    char *s;
    size_t n, cap;
} Text;

static void tprintf(Text *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void tprintf(Text *t, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (k < 0)
        return;
    if (t->n + (size_t)k + 1 > t->cap) {
        size_t cap = (t->n + (size_t)k + 1) * 2;
        char *s = realloc(t->s, cap);
        if (!s)
            return;
        t->s = s, t->cap = cap;
    }
    va_start(ap, fmt);
    vsnprintf(t->s + t->n, t->cap - t->n, fmt, ap);
    va_end(ap);
    t->n += (size_t)k;
}

static void save_lists(Store *st)
{
    char path[PATHN];
    Text t = {0};
    for (int pin = 1;; pin++) {   /* in their order */
        int more = 0;
        for (int i = 0; i < st->n; i++) {
            if (!st->e[i].gone && st->e[i].pin == pin)
                tprintf(&t, "%s\n", st->e[i].file);
            more |= !st->e[i].gone && st->e[i].pin > pin;
        }
        if (!more)
            break;
    }
    join(path, sizeof path, st->dir, "pins.txt");
    if (t.n || exists(path))
        pc_file_write_atomic(path, t.s ? t.s : "", t.n);
    t.n = 0;
    for (int s = 0; s < PC_PAK_NOTES; s++)   /* the slots last given (or read from pak.txt, before a pak is built) */
        for (int i = 0; i < st->n; i++)
            if (!st->e[i].gone && st->e[i].last == s + 1) {
                tprintf(&t, "%d\t%s\n", s, st->e[i].file);
                break;
            }
    join(path, sizeof path, st->dir, "pak.txt");
    pc_file_write_atomic(path, t.s ? t.s : "", t.n);
    free(t.s);
}

static void read_lists(Store *st)
{
    char path[PATHN], line[PC_STORE_NAME + 16];
    join(path, sizeof path, st->dir, "pins.txt");
    FILE *f = fopen(path, "r");
    int pin = 0;
    while (f && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        int i = find_entry(st, line);
        if (i >= 0 && !st->e[i].pin && st->e[i].info.kind == PC_NOTE_GHOST)
            st->e[i].pin = ++pin;
    }
    if (f)
        fclose(f);
    join(path, sizeof path, st->dir, "pak.txt");
    f = fopen(path, "r");
    while (f && fgets(line, sizeof line, f)) {
        int s;
        char *tab = strchr(line, '\t');
        line[strcspn(line, "\r\n")] = 0;
        if (!tab || sscanf(line, "%d", &s) != 1 || s < 0 || s >= PC_PAK_NOTES)
            continue;
        int i = find_entry(st, tab + 1);
        if (i >= 0 && !st->e[i].last)
            st->e[i].last = s + 1;
    }
    if (f)
        fclose(f);
}

/* to trash/ under its own name (the date added when the name is taken); the entry dropped by the caller */
static int to_trash(Store *st, Entry *e, const char *why)
{
    char from[PATHN], trash[PATHN], moved[PATHN];
    join(from, sizeof from, st->dir, e->file);
    join(trash, sizeof trash, st->dir, "trash");
    if (!exists(from)) {
        e->gone = 1;
        return 0;
    }
    if (move_into(from, trash, e->file, moved, sizeof moved) != 0)
        return -1;
    fprintf(stderr, "rush2049pc: %s: %s %s: moved to %s\n", st->label, e->file, why, moved);
    e->gone = 1;
    return 0;
}

/* ---------------------------------------------------------------- at start: backups, damaged files, the .mpk */

static void backup_name(char *out, size_t n, const char *dir, const char *file, int i)
{
    char name[PC_STORE_NAME + 16];
    if (i <= 1)
        snprintf(name, sizeof name, "backup" SEP "%s.bak", file);
    else
        snprintf(name, sizeof name, "backup" SEP "%s.bak%d", file, i);
    join(out, n, dir, name);
}

/* As saves.c does for the .mpk: a good file that differs from its .bak is copied there, .bak to .bak2, .bak2 to
 * .bak3 (the oldest goes); an unchanged one makes no backup. */
static void backup(Store *st, const char *file, const uint8_t *b, size_t size)
{
    char bak[PATHN], from[PATHN], to[PATHN], dir[PATHN];
    uint8_t *old;
    size_t n;
    backup_name(bak, sizeof bak, st->dir, file, 1);
    if (read_file(bak, &old, &n, MAX_FILE) == 0) {
        int same = n == size && !memcmp(old, b, size);
        free(old);
        if (same)
            return;
    }
    join(dir, sizeof dir, st->dir, "backup");
    if (mkdirs(dir) != 0)
        return;
    for (int i = BACKUPS; i > 1; i--) {
        backup_name(from, sizeof from, st->dir, file, i - 1);
        backup_name(to, sizeof to, st->dir, file, i);
        if (exists(from))
            pc_rename_replace(from, to);
    }
    pc_file_write_atomic(bak, b, size);
}

/* A file's note as the folder's files and their backups are taken: pc_save_file_read's checks, and for a ghost the
 * import's (pc_ghost_check, SAVES.md 4.3): a ghost someone puts in the folder itself goes on the pak too. */
static int read_note(const uint8_t *b, size_t n, PcNote *note, PcSaveFileInfo *fi, char *why, size_t why_size)
{
    if (pc_save_file_read(b, n, note, fi, why, why_size) != 0)
        return -1;
    if (fi->kind == PC_NOTE_GHOST &&
        pc_ghost_check(note->data, (size_t)note->pages * PC_PAK_PAGE, 1, why, why_size) != 0) {
        pc_note_free(note);
        return -1;
    }
    return 0;
}

/* A file that can't be read as a save: renamed FILE.corrupt-DATE, unchanged; the newest good backup put back. */
static int damaged(Store *st, const char *file, const char *why, PcNote *note, PcSaveFileInfo *fi)
{
    char path[PATHN], kept[PATHN], date[32], bak[PATHN], w[200];
    join(path, sizeof path, st->dir, file);
    local_time("%Y-%m-%d", date, sizeof date);
    fprintf(stderr, "rush2049pc: %s: %s can't be used: %s\n", st->label, file, why);
    for (int k = 1; k < 1000; k++) {
        char name[PC_STORE_NAME + 48];
        if (k == 1)
            snprintf(name, sizeof name, "%s.corrupt-%s", file, date);
        else
            snprintf(name, sizeof name, "%s.corrupt-%s-%d", file, date, k);
        join(kept, sizeof kept, st->dir, name);
        if (!exists(kept))
            break;
    }
    if (rename(path, kept) != 0) {
        fprintf(stderr, "rush2049pc: %s: it can't be set aside (%s): it is left as it is, and not used\n", st->label,
                strerror(errno));
        return -1;
    }
    fprintf(stderr, "rush2049pc: %s: it is kept, unchanged, as %s\n", st->label, kept);
    for (int i = 1; i <= BACKUPS; i++) {
        uint8_t *b;
        size_t n;
        backup_name(bak, sizeof bak, st->dir, file, i);
        if (read_file(bak, &b, &n, MAX_FILE) != 0)
            continue;
        int ok = n <= MAX_FILE && read_note(b, n, note, fi, w, sizeof w) == 0;
        if (ok && pc_file_write_atomic(path, b, n) == 0) {
            free(b);
            fprintf(stderr, "rush2049pc: %s: %s restored from %s\n", st->label, file, bak);
            return 0;
        }
        if (ok)
            pc_note_free(note);
        free(b);
    }
    fprintf(stderr, "rush2049pc: %s: there is no good backup of %s\n", st->label, file);
    return -1;
}

static int load_file(Store *st, const char *file)
{
    char path[PATHN], why[200];
    uint8_t *b = NULL;
    size_t n = 0;
    PcNote note;
    PcSaveFileInfo fi;
    int r;
    join(path, sizeof path, st->dir, file);
    if (read_file(path, &b, &n, MAX_FILE) != 0)
        r = (snprintf(why, sizeof why, "it can't be read: %s", strerror(errno)), -1);
    else if (n > MAX_FILE)
        r = (snprintf(why, sizeof why, "it is bigger than any note"), -1);
    else
        r = read_note(b, n, &note, &fi, why, sizeof why);
    if (r == 0)
        backup(st, file, b, n);
    free(b);
    if (r != 0 && damaged(st, file, why, &note, &fi) != 0)
        return -1;
    int i = add_entry(st, &note, fi.origin, file);
    pc_note_free(&note);
    return i;
}

static void read_page0(Store *st)
{
    char path[PATHN];
    uint8_t *b;
    size_t n;
    join(path, sizeof path, st->dir, "pak-id.bin");
    st->have_page0 = 0;
    if (read_file(path, &b, &n, PC_PAK_PAGE) != 0)
        return;
    if (n == PC_PAK_PAGE && pc_pak_id_valid(b + 0x20, 0)) {
        memcpy(st->page0, b, PC_PAK_PAGE);
        st->have_page0 = 1;
    } else {
        fprintf(stderr, "rush2049pc: %s: pak-id.bin is not a pak's ID: a new one is made\n", st->label);
    }
    free(b);
}

static void write_page0(Store *st)
{
    char path[PATHN];
    join(path, sizeof path, st->dir, "pak-id.bin");
    pc_file_write_atomic(path, st->page0, PC_PAK_PAGE);
}

/* The first start with files: the .mpk's notes, each into a file of its own at its slot, its ID kept; the .mpk is
 * only read. Once: imported.txt says it was. */
static void migrate(Store *st, const char *mpk, char *msg, size_t msg_size)
{
    char why[200], marker[PATHN];
    uint8_t *img;
    size_t n;
    if (read_file(mpk, &img, &n, PC_PAK_SIZE) != 0) {
        if (errno != ENOENT)
            fprintf(stderr, "rush2049pc: %s: the controller pak %s can't be read (%s): nothing imported\n", st->label,
                    mpk, strerror(errno));
        return;
    }
    if (n > PC_PAK_SIZE ? (snprintf(why, sizeof why, "it is bigger than 32 KiB"), 1)
                        : pc_pak_image_check(img, n, why, sizeof why) != PC_PAK_IMAGE_OK) {
        fprintf(stderr, "rush2049pc: %s: the controller pak %s is not imported: %s\n", st->label, mpk, why);
        free(img);
        return;
    }
    PcNote notes[PC_PAK_NOTES];
    int bad, k = pc_pak_notes(img, notes, &bad, why, sizeof why);
    if (k < 0) {
        fprintf(stderr, "rush2049pc: %s: the controller pak %s is not imported: %s\n", st->label, mpk, why);
        free(img);
        return;
    }
    memcpy(st->page0, img, PC_PAK_PAGE);
    st->have_page0 = 1;
    write_page0(st);
    free(img);
    char date[32];
    Text t = {0};
    local_time("%Y-%m-%d %H:%M:%S", date, sizeof date);
    tprintf(&t, "Imported from %s on %s: %d note%s, the .mpk left as it was.\n", mpk, date, k, k == 1 ? "" : "s");
    int done = 0;
    for (int i = 0; i < k; i++) {
        char file[PC_STORE_NAME];
        unique_file(st, &notes[i], file, sizeof file);
        int e = add_entry(st, &notes[i], PC_ORIGIN_GAME, file);
        if (e >= 0 && write_note(st, file, &notes[i], PC_ORIGIN_GAME) == 0) {
            st->e[e].last = notes[i].slot + 1;
            done++;
            tprintf(&t, "slot %d\t%s\n", notes[i].slot, file);
        } else {
            fprintf(stderr, "rush2049pc: %s: %s can't be written: %s\n", st->label, file, strerror(errno));
            if (e >= 0)
                st->e[e].gone = 1;
        }
        pc_note_free(&notes[i]);
    }
    if (bad)
        tprintf(&t, "%d damaged note%s left out (its pages don't chain).\n", bad, bad == 1 ? "" : "s");
    drop_gone(st);
    join(marker, sizeof marker, st->dir, "imported.txt");
    if (done == k && t.s)
        pc_file_write_atomic(marker, t.s, t.n);
    free(t.s);
    for (int i = 0; i < PC_PAK_NOTES; i++)
        st->on[i] = -1;
    save_lists(st);
    fprintf(stderr, "rush2049pc: %s: imported %d of %d notes of %s (the .mpk is left as it was)%s\n", st->label, done,
            k, mpk, bad ? "; damaged notes left out" : "");
    for (int i = 0; i < st->n; i++)
        fprintf(stderr, "rush2049pc: %s:   %s\n", st->label, st->e[i].file);
    if (msg)
        snprintf(msg, msg_size, "imported %d notes from %s", done, mpk);
}

/* ---------------------------------------------------------------- imports */

static int same_note_entry(Store *st, const PcNote *note)
{
    for (int i = 0; i < st->n; i++)
        if (!st->e[i].gone && pc_note_equal(&st->e[i].note, note))
            return i;
    return -1;
}

static int next_pin(Store *st)
{
    int most = 0;
    for (int i = 0; i < st->n; i++)
        if (!st->e[i].gone && st->e[i].pin > most)
            most = st->e[i].pin;
    return most + 1;
}

/* One ghost file into the library: 0 imported, 1 already there, -1 refused (why). */
static int import_one(Store *st, const char *path, char *why, size_t why_size, char *file_out, size_t file_size)
{
    uint8_t *b;
    size_t n;
    PcNote note;
    PcSaveFileInfo fi;
    char file[PC_STORE_NAME];
    if (read_file(path, &b, &n, MAX_FILE) != 0) {
        snprintf(why, why_size, "it can't be read: %s", strerror(errno));
        return -1;
    }
    if (n > MAX_FILE) {
        snprintf(why, why_size, "it is bigger than a note can be (%d bytes)", MAX_FILE);
        free(b);
        return -1;
    }
    int r = pc_save_file_read(b, n, &note, &fi, why, why_size);
    free(b);
    if (r != 0)
        return -1;
    if (fi.kind != PC_NOTE_GHOST || pc_note_game_code(&note) != PC_RUSH_GAME_CODE ||
        pc_note_company(&note) != PC_RUSH_COMPANY) {
        snprintf(why, why_size, "it is not a Rush 2049 ghost (only ghosts are imported)");
        pc_note_free(&note);
        return -1;
    }
    if (pc_ghost_check(note.data, (size_t)note.pages * PC_PAK_PAGE, 1, why, why_size) != 0) {
        pc_note_free(&note);
        return -1;
    }
    note.slot = -1;
    int have = same_note_entry(st, &note);
    if (have >= 0) {
        snprintf(file_out, file_size, "%s", st->e[have].file);
        pc_note_free(&note);
        return 1;
    }
    unique_file(st, &note, file, sizeof file);
    int i = add_entry(st, &note, PC_ORIGIN_IMPORT, file);
    if (i < 0 || write_note(st, file, &note, PC_ORIGIN_IMPORT) != 0) {
        snprintf(why, why_size, "%s can't be written: %s", file, strerror(errno));
        if (i >= 0)
            st->e[i].gone = 1;
        drop_gone(st);
        pc_note_free(&note);
        return -1;
    }
    pc_note_free(&note);
    st->e[i].pin = next_pin(st);   /* an import goes on the pak first */
    snprintf(file_out, file_size, "%s", file);
    return 0;
}

static int selection_differs(Store *st);

/* import/'s files: each imported (to import/done/) or refused (to import/refused/), with a line each. */
static int take_imports(Store *st, int *refused_out, char *last, size_t last_size)
{
    char dir[PATHN], path[PATHN], why[240], file[PC_STORE_NAME], sub[PATHN];
    char **names;
    int got = 0, refused = 0;
    join(dir, sizeof dir, st->dir, "import");
    int k = list_dir(dir, 1, &names);
    for (int i = 0; i < k; i++) {
        join(path, sizeof path, dir, names[i]);
        int r = import_one(st, path, why, sizeof why, file, sizeof file);
        if (r >= 0) {
            fprintf(stderr, "rush2049pc: %s: %s %s as %s\n", st->label, names[i],
                    r == 0 ? "imported" : "is already in the library", file);
            join(sub, sizeof sub, dir, "done");
            got += r == 0;
            if (last && r == 0)
                snprintf(last, last_size, "Imported %s", file);
        } else {
            fprintf(stderr, "rush2049pc: %s: %s refused: %s\n", st->label, names[i], why);
            join(sub, sizeof sub, dir, "refused");
            refused++;
            if (last)
                snprintf(last, last_size, "Refused %s: %s", names[i], why);
        }
        if (move_into(path, sub, names[i], NULL, 0) != 0)
            fprintf(stderr, "rush2049pc: %s: %s can't be moved out of import/: %s\n", st->label, names[i],
                    strerror(errno));
    }
    free_list(names, k);
    if (refused_out)
        *refused_out = refused;
    if (got) {
        save_lists(st);
        st->swap |= st->built && selection_differs(st);
    }
    return got;
}

/* ---------------------------------------------------------------- the pak the game sees */

/* An entry's directory entry as it goes on the pak: an import's with the game's prefix and ghost extension, and its
 * name's last two characters the digits of k (the game's own ghost names end in its direction letter). */
static void place(const Entry *e, int k, uint8_t out[32])
{
    memcpy(out, e->note.dir, 32);
    out[6] = out[7] = 0;
    if (e->origin != PC_ORIGIN_IMPORT)
        return;
    uint8_t *name = out + 16;
    out[8] |= 0x02;   /* DIR_STATUS_OCCUPIED: the game's Pfs C reads only a note it has written (osPfsReadWriteFile) */
    if (g_names) {
        memcpy(name, g_prefix, 8);
        memcpy(out + 12, g_gext, 4);
    }
    int len = 0;
    while (len < 16 && name[len])
        len++;
    for (; len < 10; len++)
        name[len] = 0x10;   /* the pak's '0' */
    name[len - 2] = (uint8_t)(0x10 + k / 10 % 10);
    name[len - 1] = (uint8_t)(0x10 + k % 10);
}

typedef struct {
    int idx[PC_PAK_NOTES];
    uint8_t dir[PC_PAK_NOTES][32];
    int n, pages, imports;
} Choice;

static int try_add(Store *st, Choice *c, int i, int reserve)
{
    Entry *e = &st->e[i];
    uint8_t d[32];
    if (e->gone || e->deleting)
        return 0;
    for (int k = 0; k < c->n; k++)
        if (c->idx[k] == i)
            return 0;
    place(e, c->imports, d);
    for (int k = 0; k < c->n; k++)
        if (pc_note_same_key(c->dir[k], d))
            return 0;   /* the same note on a pak: one of them */
    int notes = PC_PAK_NOTES - (reserve ? RESERVE_NOTES : 0), pages = PC_PAK_DATA_PAGES - (reserve ? RESERVE_PAGES : 0);
    if (c->n + 1 > notes || c->pages + e->note.pages > pages)
        return 0;
    c->idx[c->n] = i;
    memcpy(c->dir[c->n], d, 32);
    c->n++;
    c->pages += e->note.pages;
    c->imports += e->origin == PC_ORIGIN_IMPORT;
    return 1;
}

static int want_slot(const Entry *e)
{
    return e->last ? e->last - 1 : e->note.slot;
}

static Store *g_sort_st;
static int by_slot_then_file(const void *a, const void *b)
{
    const Entry *x = &g_sort_st->e[*(const int *)a], *y = &g_sort_st->e[*(const int *)b];
    int sx = want_slot(x), sy = want_slot(y);
    sx = sx < 0 ? 99 : sx, sy = sy < 0 ? 99 : sy;
    return sx != sy ? sx - sy : strcmp(x->file, y->file);
}

/* A race time to sort by: one that isn't a number (a hand-made file's) last. */
static float race_time(float t)
{
    return t == t && t < 1e30f ? t : 1e30f;
}

static int by_race(const void *a, const void *b)
{
    const Entry *x = &g_sort_st->e[*(const int *)a], *y = &g_sort_st->e[*(const int *)b];
    float tx = race_time(x->info.finish), ty = race_time(y->info.finish);
    if (x->info.track != y->info.track)
        return x->info.track - y->info.track;
    if (x->info.backward != y->info.backward)
        return x->info.backward - y->info.backward;
    if (tx != ty)
        return tx < ty ? -1 : 1;
    return strcmp(x->file, y->file);
}

static void choose(Store *st, Choice *c)
{
    int *order = malloc((size_t)(st->n ? st->n : 1) * sizeof *order), k = 0;
    memset(c, 0, sizeof *c);
    if (!order)
        return;
    g_sort_st = st;
    for (int i = 0; i < st->n; i++)   /* 1: profiles and other games' notes, by their slot */
        if (st->e[i].info.kind != PC_NOTE_GHOST)
            order[k++] = i;
    qsort(order, (size_t)k, sizeof *order, by_slot_then_file);
    for (int j = 0; j < k; j++)
        try_add(st, c, order[j], 0);
    for (int pin = 1; pin <= st->n; pin++)   /* 2: the pins, in their order */
        for (int i = 0; i < st->n; i++)
            if (st->e[i].pin == pin)
                try_add(st, c, i, 0);
    k = 0;
    for (int i = 0; i < st->n; i++)   /* 3: the ghosts on the pak last time */
        if (st->e[i].info.kind == PC_NOTE_GHOST && st->e[i].last)
            order[k++] = i;
    qsort(order, (size_t)k, sizeof *order, by_slot_then_file);
    for (int j = 0; j < k; j++)
        try_add(st, c, order[j], 0);
    k = 0;
    for (int i = 0; i < st->n; i++) {   /* 4: each player's fastest per track and way, while room is left */
        const Entry *e = &st->e[i];
        if (e->info.kind != PC_NOTE_GHOST || e->gone || e->deleting)
            continue;
        int best = 1;
        for (int j = 0; j < st->n && best; j++) {
            const Entry *o = &st->e[j];
            if (j == i || o->info.kind != PC_NOTE_GHOST || o->gone || o->deleting || o->info.track != e->info.track ||
                o->info.backward != e->info.backward || o->info.id[0] != e->info.id[0] ||
                o->info.id[1] != e->info.id[1] || strcmp(o->info.name, e->info.name) ||
                (o->origin == PC_ORIGIN_IMPORT) != (e->origin == PC_ORIGIN_IMPORT))
                continue;
            float to = race_time(o->info.finish), te = race_time(e->info.finish);
            best = to > te || (to == te && j > i);
        }
        if (best)
            order[k++] = i;
    }
    qsort(order, (size_t)k, sizeof *order, by_race);
    for (int j = 0; j < k; j++)
        try_add(st, c, order[j], 1);
    free(order);
}

static int selection_differs(Store *st)
{
    Choice c;
    choose(st, &c);
    int on = 0;
    for (int s = 0; s < PC_PAK_NOTES; s++)
        on += st->on[s] >= 0;
    if (on != c.n)
        return 1;
    for (int k = 0; k < c.n; k++) {
        int i = c.idx[k];
        if (st->e[i].slot < 0 || memcmp(st->e[i].placed, c.dir[k], 32))
            return 1;
    }
    for (int i = 0; i < st->n; i++)
        if (st->e[i].deleting)
            return 1;
    return 0;
}

static int build(Store *st, uint8_t *img, int apply, char *why, size_t why_size)
{
    Choice c;
    PcNote notes[PC_PAK_NOTES];
    const PcNote *ptr[PC_PAK_NOTES];
    int slots[PC_PAK_NOTES];
    if (!st->have_page0) {
        pc_pak_format(img);
        pc_pak_own_id(img, st->ch);
        memcpy(st->page0, img, PC_PAK_PAGE);
        st->have_page0 = 1;
        if (apply)
            write_page0(st);
    }
    if (apply) {
        for (int i = 0; i < st->n; i++)
            if (st->e[i].deleting && to_trash(st, &st->e[i], "deleted from the Saves page") != 0)
                fprintf(stderr, "rush2049pc: %s: %s can't be moved to the trash: %s\n", st->label, st->e[i].file,
                        strerror(errno));
        drop_gone(st);
    }
    choose(st, &c);
    for (int k = 0; k < c.n; k++) {
        const Entry *e = &st->e[c.idx[k]];
        notes[k] = e->note;   /* shares its data */
        memcpy(notes[k].dir, c.dir[k], 32);
        notes[k].slot = want_slot(e);
        ptr[k] = &notes[k];
    }
    if (pc_pak_build(img, st->page0, ptr, c.n, slots, why, why_size) != 0)
        return -1;
    if (!apply)
        return 0;
    for (int i = 0; i < st->n; i++)
        st->e[i].slot = -1;
    for (int s = 0; s < PC_PAK_NOTES; s++) {
        st->on[s] = slots[s] >= 0 ? c.idx[slots[s]] : -1;
        if (slots[s] >= 0) {
            Entry *e = &st->e[c.idx[slots[s]]];
            e->slot = s;
            memcpy(e->placed, c.dir[slots[s]], 32);
        }
    }
    for (int i = 0; i < st->n; i++)
        st->e[i].last = st->e[i].slot + 1;
    st->built = 1;
    st->swap = 0;
    save_lists(st);
    int profiles = 0, ghosts = 0, all_ghosts = 0, all_profiles = 0;
    for (int i = 0; i < st->n; i++) {
        all_ghosts += st->e[i].info.kind == PC_NOTE_GHOST;
        all_profiles += st->e[i].info.kind == PC_NOTE_PROFILE;
        ghosts += st->e[i].slot >= 0 && st->e[i].info.kind == PC_NOTE_GHOST;
        profiles += st->e[i].slot >= 0 && st->e[i].info.kind == PC_NOTE_PROFILE;
    }
    fprintf(stderr, "rush2049pc: %s: the pak holds %d note%s in %d pages: %d of %d profiles, %d of %d ghosts\n",
            st->label, c.n, c.n == 1 ? "" : "s", c.pages, profiles, all_profiles, ghosts, all_ghosts);
    if (profiles < all_profiles)
        fprintf(stderr, "rush2049pc: %s: %d profiles don't fit on the pak the game sees (16 notes, 123 pages)\n",
                st->label, all_profiles - profiles);
    return 0;
}

/* ---------------------------------------------------------------- the interface */

void pc_store_set_game_names(const uint8_t prefix[8], const uint8_t ghost_ext[4])
{
    pthread_mutex_lock(&g_lock);
    g_names = prefix && ghost_ext;
    if (g_names) {
        memcpy(g_prefix, prefix, 8);
        memcpy(g_gext, ghost_ext, 4);
    }
    pthread_mutex_unlock(&g_lock);
}

static void close_store(Store *st)
{
    for (int i = 0; i < st->n; i++)
        pc_note_free(&st->e[i].note);
    free(st->e);
    memset(st, 0, sizeof *st);
}

void pc_store_close(int ch)
{
    if (ch < 0 || ch > 3)
        return;
    pthread_mutex_lock(&g_lock);
    close_store(&g_st[ch]);
    pthread_mutex_unlock(&g_lock);
}

static int open_locked(Store *st, int ch, const char *dir, const char *mpk, const char *writer, char *msg,
                       size_t msg_size);

int pc_store_open(int ch, const char *dir, const char *mpk, const char *writer, char *msg, size_t msg_size)
{
    if (ch < 0 || ch > 3 || !dir)
        return -1;
    pthread_mutex_lock(&g_lock);
    int r = open_locked(&g_st[ch], ch, dir, mpk, writer, msg, msg_size);
    pthread_mutex_unlock(&g_lock);
    return r;
}

void pc_store_configure(int ch, const char *dir, const char *mpk, const char *writer)
{
    if (ch < 0 || ch > 3 || !dir)
        return;
    pthread_mutex_lock(&g_lock);
    Store *st = &g_st[ch];
    close_store(st);
    st->ch = ch;
    st->configured = 1;
    snprintf(st->dir, sizeof st->dir, "%s", dir);
    snprintf(st->mpk, sizeof st->mpk, "%s", mpk ? mpk : "");
    snprintf(st->writer, sizeof st->writer, "%s", writer ? writer : "");
    pthread_mutex_unlock(&g_lock);
}

/* Two spellings of one folder, as main gives them (pc_app_data_file's): '/' and '\\' alike, a trailing one ignored,
 * and on Windows any case. */
static int same_dir(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = *a == '\\' ? '/' : (unsigned char)*a, y = *b == '\\' ? '/' : (unsigned char)*b;
#ifdef _WIN32
        x = x >= 'A' && x <= 'Z' ? x + 32 : x, y = y >= 'A' && y <= 'Z' ? y + 32 : y;
#endif
        if (x != y)
            return (x == '/' && !a[1] && !y) || (y == '/' && !b[1] && !x);
        if (!x)
            return 1;
    }
}

static int open_locked(Store *st, int ch, const char *dir, const char *mpk, const char *writer, char *msg,
                       size_t msg_size)
{
    char path[PATHN], migrated[200] = "", dir_copy[PATHN], mpk_copy[PATHN], writer_copy[33];
    snprintf(dir_copy, sizeof dir_copy, "%s", dir);   /* they may be the store's own */
    snprintf(mpk_copy, sizeof mpk_copy, "%s", mpk ? mpk : "");
    snprintf(writer_copy, sizeof writer_copy, "%s", writer ? writer : "");
    dir = dir_copy, mpk = mpk_copy, writer = writer_copy;
    close_store(st);
    st->ch = ch;
    st->configured = 1;
    snprintf(st->mpk, sizeof st->mpk, "%s", mpk);
    snprintf(st->dir, sizeof st->dir, "%s", dir);
    snprintf(st->label, sizeof st->label, "controller %d's saves", ch + 1);
    snprintf(st->writer, sizeof st->writer, "%s", writer ? writer : "");
    for (int s = 0; s < PC_PAK_NOTES; s++)
        st->on[s] = -1;
    for (int o = 0; o < 4; o++)   /* one folder, one controller: the lowest that has it (pc_store_write finds it by dir) */
        if (&g_st[o] != st && (g_st[o].open || (o < ch && g_st[o].configured)) && same_dir(g_st[o].dir, dir)) {
            if (msg)
                snprintf(msg, msg_size, "the saves folder %s is controller %d's: give each its own", dir, o + 1);
            st->open = 0;
            return -1;
        }
    if (mkdirs(dir) != 0) {
        int err = errno;
        if (msg)
            snprintf(msg, msg_size, "the saves folder %s can't be made: %s", dir, strerror(err));
        st->open = 0;
        return -1;
    }
    static const char *const subs[] = {"import", "export", "trash", "backup"};
    for (size_t i = 0; i < sizeof subs / sizeof *subs; i++) {
        join(path, sizeof path, dir, subs[i]);
        mkdirs(path);
    }
    read_page0(st);
    char **names;
    int k = list_dir(dir, 0, &names);
    join(path, sizeof path, dir, "imported.txt");
    if (!k && mpk && *mpk && !exists(path))
        migrate(st, mpk, migrated, sizeof migrated);
    for (int i = 0; i < k; i++)
        load_file(st, names[i]);
    free_list(names, k);
    read_lists(st);
    int refused = 0, got = take_imports(st, &refused, st->notice, sizeof st->notice);
    if (got + refused > 1)
        snprintf(st->notice, sizeof st->notice, "At start: %d imported, %d refused (the log says why)", got, refused);
    st->open = 1;
    int profiles = 0, ghosts = 0, imports = 0;
    for (int i = 0; i < st->n; i++) {
        profiles += st->e[i].info.kind == PC_NOTE_PROFILE;
        ghosts += st->e[i].info.kind == PC_NOTE_GHOST;
        imports += st->e[i].origin == PC_ORIGIN_IMPORT;
    }
    if (msg)
        snprintf(msg, msg_size, "%d profile%s, %d ghost%s (%d imported)%s%s%s", profiles, profiles == 1 ? "" : "s",
                 ghosts, ghosts == 1 ? "" : "s", imports, migrated[0] ? "; " : "", migrated,
                 got || refused ? (refused ? "; ghosts refused from import/ (see the log)" : "; ghosts taken from import/")
                                : "");
    return 0;
}

int pc_store_is_open(int ch)
{
    pthread_mutex_lock(&g_lock);
    int r = store_of(ch) != NULL;
    pthread_mutex_unlock(&g_lock);
    return r;
}

const char *pc_store_dir(int ch)
{
    return ch >= 0 && ch < 4 && (g_st[ch].open || g_st[ch].configured) ? g_st[ch].dir : NULL;
}

int pc_store_build(int ch, uint8_t *img, char *why, size_t why_size)
{
    if (why && why_size)
        why[0] = 0;
    pthread_mutex_lock(&g_lock);
    if (ch >= 0 && ch < 4 && !g_st[ch].open && g_st[ch].configured) {   /* chosen later in the F1 menu: opened now */
        char msg[400];
        Store *c = &g_st[ch];
        if (open_locked(c, ch, c->dir, c->mpk, c->writer, msg, sizeof msg) == 0)
            fprintf(stderr, "rush2049pc: saves (controller %d's, files): %s: %s\n", ch + 1, c->dir, msg);
        else if (why)
            snprintf(why, why_size, "%s", msg);
    }
    Store *st = store_of(ch);
    int r = st ? build(st, img, 1, why, why_size) : -1;
    if (!st && why && !why[0])
        snprintf(why, why_size, "no saves folder");
    pthread_mutex_unlock(&g_lock);
    return r;
}

int pc_store_swap_wanted(int ch)
{
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int r = st && st->built && st->swap;
    pthread_mutex_unlock(&g_lock);
    return r;
}

/* The image's note at a slot against the entry there: the same bytes as placed. */
static int as_placed(const Entry *e, const PcNote *in)
{
    return !memcmp(in->dir, e->placed, 32) && in->pages == e->note.pages &&
           !memcmp(in->data, e->note.data, (size_t)in->pages * PC_PAK_PAGE);
}

int pc_store_write(const char *dir, const void *image, size_t size)
{
    PcNote in[PC_PAK_NOTES];
    char why[200];
    int err = 0, changed = 0;
    pthread_mutex_lock(&g_lock);
    Store *st = NULL;
    for (int ch = 0; ch < 4 && !st; ch++)
        if (g_st[ch].open && !strcmp(g_st[ch].dir, dir))
            st = &g_st[ch];
    if (!st || size != PC_PAK_SIZE) {
        pthread_mutex_unlock(&g_lock);
        return 0;
    }
    int bad, k = pc_pak_notes(image, in, &bad, why, sizeof why);
    if (k < 0) {   /* the game's own writes keep the tables good: nothing here to take */
        fprintf(stderr, "rush2049pc: %s: the pak's %s: nothing taken from it\n", st->label, why);
        pthread_mutex_unlock(&g_lock);
        return 0;
    }
    int at[PC_PAK_NOTES];
    for (int s = 0; s < PC_PAK_NOTES; s++)
        at[s] = -1;
    for (int i = 0; i < k; i++)
        at[in[i].slot] = i;
    for (int s = 0; s < PC_PAK_NOTES; s++) {
        PcNote *note = at[s] >= 0 ? &in[at[s]] : NULL;
        int ei = st->on[s];
        if (ei >= 0) {
            Entry *e = &st->e[ei];
            if (note && as_placed(e, note))
                continue;
            int same_pages = note && note->pages == e->note.pages &&
                             !memcmp(note->data, e->note.data, (size_t)note->pages * PC_PAK_PAGE);
            if (note && pc_note_same_key(note->dir, e->placed) && (e->info.kind != PC_NOTE_GHOST || same_pages)) {
                /* changed in place (a profile's settings, records; a note's status byte, which the Pfs C sets after
                 * its first write): its file written again */
                PcNote next;
                if (pc_note_copy(&next, note) != 0) {
                    err = ENOMEM;
                    continue;
                }
                next.slot = s;
                if (write_note(st, e->file, &next, e->origin) != 0) {
                    err = errno;
                    pc_note_free(&next);
                    continue;
                }
                pc_note_free(&e->note);
                e->note = next;
                pc_note_info(&e->note, &e->info);
                memcpy(e->placed, note->dir, 32);
                changed = 1;
                continue;
            }
            if (to_trash(st, e, note ? "was replaced on the pak by the game" : "was deleted by the game") != 0) {
                err = errno;
                continue;
            }
            st->on[s] = -1;
            e->slot = -1;
            changed = 1;
        }
        if (!note)
            continue;
        PcNoteInfo info;
        pc_note_info(note, &info);
        int rush = pc_note_game_code(note) == PC_RUSH_GAME_CODE && pc_note_company(note) == PC_RUSH_COMPANY;
        if (rush && (info.kind == PC_NOTE_OTHER || !(note->dir[8] & 0x02) ||   /* never written: DIR_STATUS_OCCUPIED */
                     (info.kind == PC_NOTE_GHOST &&
                      pc_ghost_check(note->data, (size_t)note->pages * PC_PAK_PAGE, 0, NULL, 0) != 0)))
            continue;   /* the game's note, not whole yet (being written): the next image has it */
        char file[PC_STORE_NAME];
        unique_file(st, note, file, sizeof file);
        if (write_note(st, file, note, PC_ORIGIN_GAME) != 0) {
            err = errno;
            continue;
        }
        int i = add_entry(st, note, PC_ORIGIN_GAME, file);
        if (i < 0) {
            err = ENOMEM;
            continue;
        }
        st->e[i].slot = s;
        st->e[i].last = s + 1;
        st->e[i].note.slot = s;
        st->on[s] = i;
        changed = 1;
        fprintf(stderr, "rush2049pc: %s: the game saved %s\n", st->label, file);
    }
    for (int i = 0; i < k; i++)
        pc_note_free(&in[i]);
    if (changed) {
        drop_gone(st);
        for (int i = 0; i < st->n; i++)
            st->e[i].last = st->e[i].slot + 1;
        save_lists(st);
    }
    if (err && !st->failing)
        fprintf(stderr, "rush2049pc: %s: cannot write in %s: %s (the saves are kept in memory and tried again)\n",
                st->label, st->dir, strerror(err));
    else if (!err && st->failing)
        fprintf(stderr, "rush2049pc: %s: written again\n", st->label);
    st->failing = err != 0;
    pthread_mutex_unlock(&g_lock);
    errno = err;
    return err ? -1 : 0;
}

static int by_list(const void *a, const void *b)
{
    const Entry *x = &g_sort_st->e[*(const int *)a], *y = &g_sort_st->e[*(const int *)b];
    static const int rank[] = {2, 0, 1};
    if (x->info.kind != y->info.kind)
        return rank[x->info.kind] - rank[y->info.kind];
    if (x->info.kind == PC_NOTE_GHOST)
        return by_race(a, b);
    int c = strcmp(x->info.name, y->info.name);
    return c ? c : strcmp(x->file, y->file);
}

int pc_store_list(int ch, PcStoreEntry *out, int max)
{
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int n = 0;
    int *order = st ? malloc((size_t)(st->n ? st->n : 1) * sizeof *order) : NULL;
    if (order) {
        for (int i = 0; i < st->n; i++)
            order[i] = i;
        g_sort_st = st;
        qsort(order, (size_t)st->n, sizeof *order, by_list);
        for (int j = 0; j < st->n && n < max; j++) {
            const Entry *e = &st->e[order[j]];
            PcStoreEntry *o = &out[n++];
            memset(o, 0, sizeof *o);
            snprintf(o->file, sizeof o->file, "%s", e->file);
            o->kind = e->info.kind;
            o->track = e->info.track, o->backward = e->info.backward, o->car = e->info.car;
            snprintf(o->name, sizeof o->name, "%s", e->info.name);
            o->finish = e->info.finish;
            o->pages = e->note.pages;
            o->on_pak = e->slot >= 0;
            o->pinned = e->pin > 0;
            o->imported = e->origin == PC_ORIGIN_IMPORT;
            o->deleting = e->deleting;
            if (e->info.kind == PC_NOTE_GHOST) {
                long cs = e->info.finish > 0 && e->info.finish < 36000.0f ? lrintf(e->info.finish * 100.0f) : 0;
                snprintf(o->label, sizeof o->label, "T%d %s %.13s %ld:%02ld.%02ld", e->info.track + 1,
                         e->info.backward ? "bwd" : "fwd", e->info.name, cs / 6000, cs / 100 % 60, cs % 100);
            } else if (e->info.kind == PC_NOTE_PROFILE) {
                snprintf(o->label, sizeof o->label, "%.13s", e->info.name[0] ? e->info.name : "(no name)");
            } else {
                snprintf(o->label, sizeof o->label, "note %08X", (unsigned)pc_note_game_code(&e->note));
            }
        }
    }
    free(order);
    pthread_mutex_unlock(&g_lock);
    return n;
}

int pc_store_pin(int ch, const char *file, int on, char *msg, size_t msg_size)
{
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int i = st ? find_entry(st, file) : -1, r = -1;
    if (i < 0 || st->e[i].info.kind != PC_NOTE_GHOST)
        snprintf(msg, msg_size, i < 0 ? "No such save" : "Only ghosts are pinned");
    else {
        Entry *e = &st->e[i];
        if (on && !e->pin)
            e->pin = next_pin(st);
        else if (!on)
            e->pin = 0;
        save_lists(st);
        st->swap = st->built && selection_differs(st);
        snprintf(msg, msg_size, "%s %s%s", on ? "Pinned" : "Unpinned", e->info.name,
                 st->swap ? ": the pak changes at the title or next start" : "");
        r = 0;
    }
    pthread_mutex_unlock(&g_lock);
    return r;
}

int pc_store_delete(int ch, const char *file, char *msg, size_t msg_size)
{
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int i = st ? find_entry(st, file) : -1, r = -1;
    if (i < 0) {
        snprintf(msg, msg_size, "No such save");
    } else if (st->e[i].slot >= 0) {   /* the game has it: it goes with the next swap */
        st->e[i].deleting = 1;
        st->e[i].pin = 0;
        save_lists(st);
        st->swap = st->built;
        snprintf(msg, msg_size, "Deleted: to the trash at the title or next start");
        r = 0;
    } else if (to_trash(st, &st->e[i], "deleted from the Saves page") == 0) {
        drop_gone(st);
        save_lists(st);
        st->swap = st->built && selection_differs(st);
        snprintf(msg, msg_size, "Moved to the trash folder");
        r = 0;
    } else {
        snprintf(msg, msg_size, "Not deleted: %s", strerror(errno));
    }
    pthread_mutex_unlock(&g_lock);
    return r;
}

int pc_store_export_ghost(int ch, const char *file, char *msg, size_t msg_size)
{
    char dir[PATHN], path[PATHN], name[PC_STORE_NAME];
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int i = st ? find_entry(st, file) : -1, r = -1;
    if (i < 0 || st->e[i].info.kind != PC_NOTE_GHOST) {
        snprintf(msg, msg_size, i < 0 ? "No such save" : "Only ghosts are exported");
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    PcNote note = st->e[i].note;
    note.slot = -1;
    join(dir, sizeof dir, st->dir, "export");
    mkdirs(dir);
    pc_save_file_name(&note, name, sizeof name);
    const char *dot = strrchr(name, '.');
    int k;
    for (k = 1; k < 1000; k++) {
        char want[PC_STORE_NAME + 16];
        if (k == 1)
            snprintf(want, sizeof want, "%s", name);
        else
            snprintf(want, sizeof want, "%.*s (%d)%s", (int)(dot - name), name, k, dot);
        join(path, sizeof path, dir, want);
        if (!exists(path))
            break;
    }
    if (k == 1000) {   /* never over a file */
        snprintf(msg, msg_size, "Not exported: export/ has 999 copies of it");
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    uint8_t *b;
    size_t size;
    if (pc_save_file_make(&note, PC_ORIGIN_EXPORT, st->writer, &b, &size) == 0) {
        r = pc_file_write_atomic(path, b, size);
        free(b);
    }
    if (r == 0) {
        fprintf(stderr, "rush2049pc: %s: %s exported to %s\n", st->label, file, path);
        snprintf(msg, msg_size, "Exported to export%s%s", SEP, path + strlen(dir) + 1);
    } else {
        snprintf(msg, msg_size, "Not exported: %s", strerror(errno));
    }
    pthread_mutex_unlock(&g_lock);
    return r;
}

int pc_store_notice(int ch, char *out, size_t size)
{
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int r = st && st->notice[0];
    snprintf(out, size, "%s", r ? st->notice : "");
    pthread_mutex_unlock(&g_lock);
    return r;
}

int pc_store_import(int ch, char *msg, size_t msg_size)
{
    char last[300] = "";
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int refused = 0, got = st ? take_imports(st, &refused, last, sizeof last) : -1;
    if (st)
        st->notice[0] = 0;
    if (got < 0)
        snprintf(msg, msg_size, "No saves folder");
    else if (!got && !refused)
        snprintf(msg, msg_size, "No ghost files in import%s", SEP);
    else if (got + refused == 1)
        snprintf(msg, msg_size, "%s", last);
    else
        snprintf(msg, msg_size, "%d imported, %d refused (the log says why)", got, refused);
    pthread_mutex_unlock(&g_lock);
    return got > 0 ? 0 : -1;
}

int pc_store_import_file(int ch, const char *path, char *msg, size_t msg_size)
{
    char why[240], file[PC_STORE_NAME];
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int r = st ? import_one(st, path, why, sizeof why, file, sizeof file) : -1;
    if (!st)
        snprintf(msg, msg_size, "No saves folder");
    else if (r < 0) {
        fprintf(stderr, "rush2049pc: %s: %s refused: %s\n", st->label, path, why);
        snprintf(msg, msg_size, "Refused: %s", why);
    } else {
        fprintf(stderr, "rush2049pc: %s: %s %s as %s\n", st->label, path, r == 0 ? "imported" : "is already in the library",
                file);
        snprintf(msg, msg_size, r == 0 ? "Imported %s" : "Already in the library: %s", file);
        save_lists(st);
        st->swap = st->built && selection_differs(st);
    }
    pthread_mutex_unlock(&g_lock);
    return r < 0 ? -1 : 0;
}

int pc_store_export_mpk(int ch, const char *mpk, char *msg, size_t msg_size)
{
    static uint8_t img[PC_PAK_SIZE];
    char why[200], from[PATHN], to[PATHN];
    pthread_mutex_lock(&g_lock);
    Store *st = store_of(ch);
    int r = -1;
    if (!st) {
        snprintf(msg, msg_size, "No saves folder");
    } else if (build(st, img, 0, why, sizeof why) != 0) {
        snprintf(msg, msg_size, "Not exported: %s", why);
    } else {
        for (int i = BACKUPS; i > 1; i--) {   /* as saves.c keeps the pak's backups */
            if (i == 2)
                snprintf(from, sizeof from, "%s.bak", mpk);
            else
                snprintf(from, sizeof from, "%s.bak%d", mpk, i - 1);
            snprintf(to, sizeof to, "%s.bak%d", mpk, i);
            if (exists(from))
                pc_rename_replace(from, to);
        }
        snprintf(to, sizeof to, "%s.bak", mpk);
        int kept = exists(mpk) && pc_rename_replace(mpk, to) == 0;
        r = pc_file_write_atomic(mpk, img, PC_PAK_SIZE);
        if (r == 0) {
            fprintf(stderr, "rush2049pc: %s: the pak exported to %s%s%s\n", st->label, mpk, kept ? "; the old one is " : "",
                    kept ? to : "");
            snprintf(msg, msg_size, "Wrote the .mpk%s", kept ? " (the old one is its .bak)" : "");
        } else {
            snprintf(msg, msg_size, "Not exported: %s", strerror(errno));
        }
    }
    pthread_mutex_unlock(&g_lock);
    return r;
}
