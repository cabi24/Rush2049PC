/* port/app/tests/test_saves.c: the controller pak's file (port/ultra/pakfile.c, port/app/saves.c,
 * docs/port/RELEASE.md "Saves"), in a temporary folder under the directory given: what an image is, the atomic
 * write, the backup when the pak changed and the rotation of three, a damaged pak set aside under its date and the
 * newest good backup put in its place, an N64-order pak left alone. The message box is the test's (pc_app_alert_kind
 * below): it records the text. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "ultra.h"
#include "pc_compat.h"

static int failures;
static char box_text[4096];
static int boxes;

#define CHECK(cond, ...)                                                                                          \
    do {                                                                                                          \
        if (!(cond)) {                                                                                            \
            failures++;                                                                                           \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
            printf(__VA_ARGS__);                                                                                  \
            printf("\n");                                                                                         \
        }                                                                                                         \
    } while (0)

int pc_app_alert_kind(int show, int kind, const char *title, const char *text)
{
    (void)kind, (void)title;
    if (show) {
        boxes++;
        snprintf(box_text, sizeof box_text, "%s", text);
    }
    return 0;
}

static char dir[PC_APP_PATH], pak[PC_APP_PATH];

static void path_of(char *out, const char *suffix)
{
    snprintf(out, PC_APP_PATH, "%s%s", pak, suffix);
}

static void put(const char *suffix, const void *data, size_t n)
{
    char p[PC_APP_PATH];
    path_of(p, suffix);
    FILE *f = fopen(p, "wb");
    if (!f || fwrite(data, 1, n, f) != n || fclose(f) != 0) {
        printf("cannot write %s\n", p);
        exit(1);
    }
}

/* The file's bytes (malloc'd), its size in *n; NULL when it isn't there. */
static uint8_t *get(const char *suffix, size_t *n)
{
    char p[PC_APP_PATH];
    path_of(p, suffix);
    FILE *f = fopen(p, "rb");
    if (!f)
        return NULL;
    uint8_t *b = malloc(1 << 17);
    *n = fread(b, 1, 1 << 17, f);
    fclose(f);
    return b;
}

static int has(const char *suffix, const uint8_t *want, size_t n)
{
    size_t got;
    uint8_t *b = get(suffix, &got);
    int same = b && got == n && !memcmp(b, want, n);
    free(b);
    return same;
}

static int there(const char *suffix)
{
    char p[PC_APP_PATH];
    path_of(p, suffix);
    return pc_app_exists(p);
}

static void gone(const char *suffix)
{
    char p[PC_APP_PATH];
    path_of(p, suffix);
    struct stat st;
    if (stat(p, &st) == 0 && S_ISDIR(st.st_mode))
        rmdir(p);
    else
        remove(p);
}

static void clean(void)
{
    static const char *const all[] = {"", ".bak", ".bak2", ".bak3", ".bak4", ".tmp"};
    char date[16], s[64];
    time_t t = time(NULL);
    strftime(date, sizeof date, "%Y-%m-%d", localtime(&t));
    for (size_t i = 0; i < sizeof all / sizeof *all; i++)
        gone(all[i]);
    snprintf(s, sizeof s, ".corrupt-%s", date);
    gone(s);
    for (int k = 2; k < 5; k++) {
        snprintf(s, sizeof s, ".corrupt-%s-%d", date, k);
        gone(s);
    }
    boxes = 0;
    box_text[0] = 0;
}

/* A good pak with one byte of a data page (page 5 on: no checksum covers it) set to v: a save that changed. */
static void good(uint8_t *img, int v)
{
    pc_pak_format(img);
    img[0x500] = (uint8_t)v;
}

/* The same image with the ID block's copies in N64 (big-endian) order, as an emulator writes it. */
static void n64_order(uint8_t *img)
{
    static const int copies[] = {0x20, 0x60, 0x80, 0xC0};
    for (int c = 0; c < 4; c++)
        for (int j = 0; j < 32; j += 2) {
            uint8_t t = img[copies[c] + j];
            img[copies[c] + j] = img[copies[c] + j + 1];
            img[copies[c] + j + 1] = t;
        }
}

static void t_image(void)
{
    static uint8_t img[PC_PAK_SIZE];
    char why[160];
    pc_pak_format(img);
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_OK, "formatted: %s", why);
    good(img, 7);
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_OK, "a data byte: %s", why);
    CHECK(pc_pak_image_check(img, 100, why, sizeof why) == PC_PAK_IMAGE_DAMAGED && strstr(why, "100 bytes"),
          "short: %s", why);
    CHECK(pc_pak_image_check(img, 0, why, sizeof why) == PC_PAK_IMAGE_DAMAGED && strstr(why, "empty"), "empty: %s",
          why);
    CHECK(pc_pak_image_check(img, 65536, why, sizeof why) == PC_PAK_IMAGE_DAMAGED && strstr(why, "65536"),
          "big: %s", why);
    memset(img + 0x20, 0x55, 32);   /* the first ID copy: the game repairs it from the others */
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_OK, "one ID copy bad: %s", why);
    memset(img + 0x60, 0, 32), memset(img + 0x80, 0xFF, 32), memset(img + 0xC0, 1, 32);
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_DAMAGED && strstr(why, "ID block"),
          "every ID copy bad: %s", why);
    pc_pak_format(img);
    img[0x100 + 40] ^= 1;   /* the inode table: the game takes its backup */
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_OK, "one inode table bad: %s", why);
    img[0x200 + 40] ^= 1;
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_DAMAGED && strstr(why, "file table"),
          "both inode tables bad: %s", why);
    memset(img, 0, PC_PAK_SIZE);
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_DAMAGED, "zeros: %s", why);
    pc_pak_format(img);
    n64_order(img);
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_N64 && strstr(why, "N64 byte order"),
          "N64 order: %s", why);
}

static void t_write(void)
{
    static uint8_t a[PC_PAK_SIZE], b[PC_PAK_SIZE];
    char p[PC_APP_PATH];
    good(a, 1), good(b, 2);
    clean();
    path_of(p, "");
    CHECK(pc_file_write_atomic(p, a, sizeof a) == 0 && has("", a, sizeof a), "a new file");
    CHECK(!there(".tmp"), "the temp file is gone");
    CHECK(pc_file_write_atomic(p, b, sizeof b) == 0 && has("", b, sizeof b), "replaced");
    put(".tmp", "left by a crash", 15);
    CHECK(pc_file_write_atomic(p, a, sizeof a) == 0 && has("", a, sizeof a) && !there(".tmp"),
          "a temp file a crash left");
    /* the temp file can't be made: the file is untouched */
    char tmp[PC_APP_PATH];
    path_of(tmp, ".tmp");
    mkdir(tmp, 0755);
    CHECK(pc_file_write_atomic(p, b, sizeof b) != 0 && has("", a, sizeof a), "no temp file: the old one stays");
    rmdir(tmp);
#ifndef _WIN32
    if (geteuid() != 0) {   /* a folder that can't be written: the same */
        chmod(dir, 0555);
        CHECK(pc_file_write_atomic(p, b, sizeof b) != 0 && has("", a, sizeof a) && !there(".tmp"),
              "a read-only folder: the old one stays");
        chmod(dir, 0755);
    }
#endif
    clean();
}

/* pc_pak_file_load, as si.c loads each pak file */
static void t_load(void)
{
    static uint8_t img[PC_PAK_SIZE], want[PC_PAK_SIZE], fresh[PC_PAK_SIZE], big[65536];
    char p[PC_APP_PATH], why[200];
    path_of(p, "");
    pc_pak_format(fresh);
    clean();
    memset(img, 0xAA, sizeof img);
    CHECK(pc_pak_file_load(p, img, why, sizeof why) == PC_PAK_LOAD_NEW && !memcmp(img, fresh, PC_PAK_SIZE),
          "no file: a new pak");
    good(want, 9);
    put("", want, PC_PAK_SIZE);
    CHECK(pc_pak_file_load(p, img, why, sizeof why) == PC_PAK_LOAD_FILE && !memcmp(img, want, PC_PAK_SIZE),
          "a good file: %s", why);
    n64_order(want);
    put("", want, PC_PAK_SIZE);
    CHECK(pc_pak_file_load(p, img, why, sizeof why) == PC_PAK_LOAD_N64 && !memcmp(img, fresh, PC_PAK_SIZE),
          "N64 order: a blank pak");
    put("", big, sizeof big);
    CHECK(pc_pak_file_load(p, img, why, sizeof why) == PC_PAK_LOAD_NONE && strstr(why, "65536 bytes"),
          "too big: %s", why);
#ifndef _WIN32
    if (geteuid() != 0) {
        good(want, 9);
        put("", want, PC_PAK_SIZE);
        chmod(p, 0);
        CHECK(pc_pak_file_load(p, img, why, sizeof why) == PC_PAK_LOAD_NONE && strstr(why, "cannot be read"),
              "unreadable: %s", why);
        chmod(p, 0644);
    }
#endif
    clean();
}

static void t_backups(void)
{
    static uint8_t v[5][PC_PAK_SIZE];
    for (int i = 0; i < 5; i++)
        good(v[i], i + 1);
    clean();
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_NEW && !there(".bak"), "no pak: nothing to back up");
    put("", v[0], PC_PAK_SIZE);
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_BACKED_UP && has(".bak", v[0], PC_PAK_SIZE),
          "the first backup");
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_OK && !there(".bak2"), "unchanged: no new backup");
    for (int i = 1; i < 5; i++) {
        put("", v[i], PC_PAK_SIZE);
        CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_BACKED_UP, "change %d backed up", i);
    }
    CHECK(has(".bak", v[4], PC_PAK_SIZE) && has(".bak2", v[3], PC_PAK_SIZE) && has(".bak3", v[2], PC_PAK_SIZE) &&
              !there(".bak4"), "three backups, newest first, the oldest dropped");
    CHECK(has("", v[4], PC_PAK_SIZE), "the pak itself unchanged");
    CHECK(boxes == 0, "no box for a backup");
    clean();
}

static void t_damaged(void)
{
    static uint8_t ok1[PC_PAK_SIZE], ok2[PC_PAK_SIZE], zeros[PC_PAK_SIZE], n64[PC_PAK_SIZE];
    char date[16], first[64], second[64];
    time_t t = time(NULL);
    strftime(date, sizeof date, "%Y-%m-%d", localtime(&t));
    snprintf(first, sizeof first, ".corrupt-%s", date);
    snprintf(second, sizeof second, ".corrupt-%s-2", date);
    good(ok1, 1), good(ok2, 2);

    /* a damaged pak with a good backup: kept aside, unchanged, and the backup in its place */
    clean();
    put("", zeros, PC_PAK_SIZE);
    put(".bak", ok1, PC_PAK_SIZE);
    put(".bak2", ok2, PC_PAK_SIZE);
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_RESTORED, "restored");
    CHECK(has(first, zeros, PC_PAK_SIZE), "the damaged pak kept as %s", first);
    CHECK(has("", ok1, PC_PAK_SIZE), "the newest backup in its place");
    CHECK(has(".bak", ok1, PC_PAK_SIZE) && has(".bak2", ok2, PC_PAK_SIZE), "the backups untouched");
    CHECK(boxes == 1 && strstr(box_text, "can't be used: its ID block") && strstr(box_text, first) &&
              strstr(box_text, "restored from the backup"), "the box: %s", box_text);
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_OK && !there(".bak3"), "the next start: the same as .bak");

    /* again the same day: a second name */
    put("", "short", 5);
    CHECK(pc_app_saves_prepare(pak, 0) == PC_APP_SAVES_RESTORED && has(second, (const uint8_t *)"short", 5) &&
              has(first, zeros, PC_PAK_SIZE), "a second damaged pak the same day: %s", second);
    CHECK(boxes == 1, "no box without a window");

    /* the newest backup damaged too: the next one */
    clean();
    put("", zeros, 1000);
    put(".bak", zeros, PC_PAK_SIZE);
    put(".bak2", ok2, PC_PAK_SIZE);
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_RESTORED && has("", ok2, PC_PAK_SIZE), "restored from .bak2");
    CHECK(strstr(box_text, "1000 bytes") && strstr(box_text, ".bak2"), "the box: %s", box_text);

    /* no backup: set aside, and no pak left, so the game makes a new one */
    clean();
    put("", zeros, PC_PAK_SIZE);
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_SET_ASIDE && !there("") && has(first, zeros, PC_PAK_SIZE),
          "no backup: set aside");
    CHECK(strstr(box_text, "new, empty controller pak"), "the box: %s", box_text);

    /* a folder where the pak should be */
    clean();
    char p[PC_APP_PATH];
    path_of(p, "");
    mkdir(p, 0755);
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_SET_ASIDE && !there("") && there(first), "a folder");
    CHECK(strstr(box_text, "folder"), "the box: %s", box_text);
#ifndef _WIN32
    if (geteuid() != 0) {   /* a pak that can't be read */
        clean();
        put("", ok1, PC_PAK_SIZE);
        chmod(p, 0);
        CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_SET_ASIDE && !there("") && there(first), "unreadable");
        CHECK(strstr(box_text, "Permission denied"), "the box: %s", box_text);
        char kept[PC_APP_PATH];
        path_of(kept, first);
        chmod(kept, 0644);
        CHECK(has(first, ok1, PC_PAK_SIZE), "unreadable: kept as it was");
    }
#endif

    /* N64 byte order: left alone, not backed up */
    clean();
    pc_pak_format(n64);
    n64_order(n64);
    put("", n64, PC_PAK_SIZE);
    CHECK(pc_app_saves_prepare(pak, 1) == PC_APP_SAVES_N64 && has("", n64, PC_PAK_SIZE) && !there(".bak") &&
              !there(first) && boxes == 0, "N64 order: left alone");
    clean();
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: test_saves SCRATCH_DIR\n");
        return 2;
    }
    pc_app_mkdirs(argv[1]);
    pc_app_join(dir, sizeof dir, argv[1], "saves dir");   /* a space in it */
    pc_app_mkdirs(dir);
    pc_app_join(pak, sizeof pak, dir, "rush2049pc.mpk");
    t_image();
    t_write();
    t_load();
    t_backups();
    t_damaged();
    printf("app_saves: %s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
