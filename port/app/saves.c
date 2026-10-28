/* port/app/saves.c: the controller pak's file at start, before the game reads it (docs/port/RELEASE.md, "Saves").
 *
 * The pak (rush2049pc.mpk) is the player's progress, so:
 *   - a good one is copied to rush2049pc.mpk.bak when it differs from that copy (the run before changed it); the
 *     backups before move to .bak2 and .bak3, and the oldest goes;
 *   - one the game can't use, or would write over (unreadable, not 32 KiB, its ID block or file table damaged:
 *     port/ultra/pakfile.c), is renamed rush2049pc.mpk.corrupt-DATE, unchanged, and the newest good backup is put in
 *     its place; with none, si.c makes a new, empty one. The console says what happened, and a message box when the
 *     program has a window, since the player would otherwise find the saves gone;
 *   - an image in N64 byte order is left alone (si.c says so, and uses a blank pak in memory).
 * The pak itself is written by si.c, through pakfile.c's temp file and rename. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "ultra.h"
#include "pc_compat.h"

static uint8_t g_img[PC_PAK_SIZE + 1], g_other[PC_PAK_SIZE + 1];

/* The file into buf (up to cap bytes); *size its whole size. 0, or -1 with *why saying why not. */
static int read_file(const char *path, uint8_t *buf, size_t cap, size_t *size, char *why, size_t why_size)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        snprintf(why, why_size, "it cannot be read: %s", strerror(errno));
        return -1;
    }
    if (S_ISDIR(st.st_mode)) {
        snprintf(why, why_size, "it is a folder, not a file");
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(why, why_size, "it cannot be read: %s", strerror(errno));
        return -1;
    }
    size_t want = (size_t)st.st_size < cap ? (size_t)st.st_size : cap;
    size_t got = fread(buf, 1, want, f);
    int bad = ferror(f);
    fclose(f);
    if (bad || got != want) {
        snprintf(why, why_size, "it cannot be read: %s", bad ? strerror(errno) : "it changed while it was read");
        return -1;
    }
    *size = (size_t)st.st_size;
    return 0;
}

static void backup_name(char *out, size_t size, const char *path, int i)
{
    if (i <= 1)
        snprintf(out, size, "%s.bak", path);
    else
        snprintf(out, size, "%s.bak%d", path, i);
}

static void local_time(time_t t, const char *format, char *out, size_t size)
{
    struct tm tm;
#ifdef _WIN32
    tm = *localtime(&t);
#else
    localtime_r(&t, &tm);
#endif
    strftime(out, size, format, &tm);
}

/* path renamed PATH.corrupt-DATE (then -2, -3... on the same day); kept gets the new name. */
static int set_aside(const char *path, char *kept, size_t size)
{
    char date[32];
    local_time(time(NULL), "%Y-%m-%d", date, sizeof date);
    for (int k = 1; k < 1000; k++) {
        if (k == 1)
            snprintf(kept, size, "%s.corrupt-%s", path, date);
        else
            snprintf(kept, size, "%s.corrupt-%s-%d", path, date, k);
        if (pc_app_exists(kept))
            continue;
        return rename(path, kept);   /* never over a file: the name is free */
    }
    errno = EEXIST;
    return -1;
}

/* The newest backup that is a good pak, into g_other: its number (1 for .bak), or 0. */
static int good_backup(const char *path, char *name, size_t size)
{
    char why[160];
    for (int i = 1; i <= PC_APP_BACKUPS; i++) {
        size_t n;
        backup_name(name, size, path, i);
        if (pc_app_exists(name) && read_file(name, g_other, sizeof g_other, &n, why, sizeof why) == 0 &&
            pc_pak_image_check(g_other, n, NULL, 0) == PC_PAK_IMAGE_OK)
            return i;
    }
    name[0] = 0;
    return 0;
}

static int backup(const char *path)
{
    char bak[PC_APP_PATH], from[PC_APP_PATH], to[PC_APP_PATH], why[160];
    size_t n;
    backup_name(bak, sizeof bak, path, 1);
    if (pc_app_exists(bak) && read_file(bak, g_other, sizeof g_other, &n, why, sizeof why) == 0 &&
        n == PC_PAK_SIZE && !memcmp(g_other, g_img, PC_PAK_SIZE))
        return PC_APP_SAVES_OK;   /* the same as the last backup */
    for (int i = PC_APP_BACKUPS; i > 1; i--) {
        backup_name(from, sizeof from, path, i - 1);
        backup_name(to, sizeof to, path, i);
        if (pc_app_exists(from) && pc_rename_replace(from, to) != 0)
            fprintf(stderr, "rush2049pc: saves: cannot move %s to %s: %s\n", from, to, strerror(errno));
    }
    if (pc_file_write_atomic(bak, g_img, PC_PAK_SIZE) != 0) {
        fprintf(stderr, "rush2049pc: saves: cannot back up to %s: %s\n", bak, strerror(errno));
        return PC_APP_SAVES_OK;
    }
    fprintf(stderr, "rush2049pc: saves backed up, since they changed: %s\n", bak);
    return PC_APP_SAVES_BACKED_UP;
}

static int damaged(const char *path, const char *why, int window)
{
    char kept[PC_APP_PATH], bak[PC_APP_PATH], when[32], box[4096];
    struct stat st;
    int rc;
    fprintf(stderr, "rush2049pc: the saves (the controller pak) %s can't be used: %s\n", path, why);
    int len = snprintf(box, sizeof box, "Your saves, the controller pak\n    %s\ncan't be used: %s.\n\n", path, why);
    if (set_aside(path, kept, sizeof kept) != 0) {
        fprintf(stderr, "rush2049pc: it cannot be set aside (%s): no controller pak this time, and nothing is "
                "written over it\n", strerror(errno));
        snprintf(box + len, sizeof box - (size_t)len, "It could not be renamed (%s), so the game runs without a "
                 "controller pak this time and nothing is written over the file. Move the file away, or put a "
                 "backup (%s.bak) in its place, and start again.", strerror(errno), path);
        pc_app_alert_kind(window, PC_APP_BOX_WARNING, "Rush 2049 PC: saves", box);
        return PC_APP_SAVES_LEFT;
    }
    fprintf(stderr, "rush2049pc: it is kept, unchanged, as %s\n", kept);
    len += snprintf(box + len, sizeof box - (size_t)len, "Nothing was written over it: it is kept as\n    %s\n\n",
                    kept);
    if (good_backup(path, bak, sizeof bak) && pc_file_write_atomic(path, g_other, PC_PAK_SIZE) == 0) {
        when[0] = 0;
        if (stat(bak, &st) == 0)
            local_time(st.st_mtime, "%Y-%m-%d %H:%M", when, sizeof when);
        fprintf(stderr, "rush2049pc: the saves are restored from %s (%s)\n", bak, when);
        snprintf(box + len, sizeof box - (size_t)len, "The saves were restored from the backup of %s,\n    %s\n"
                 "Anything saved after that is only in the file kept.", when, bak);
        rc = PC_APP_SAVES_RESTORED;
    } else {
        fprintf(stderr, "rush2049pc: there is no good backup: the game starts with a new, empty controller pak\n");
        snprintf(box + len, sizeof box - (size_t)len, "There is no good backup (%s.bak), so the game starts with a "
                 "new, empty controller pak.", path);
        rc = PC_APP_SAVES_SET_ASIDE;
    }
    pc_app_alert_kind(window, PC_APP_BOX_WARNING, "Rush 2049 PC: saves", box);
    return rc;
}

int pc_app_saves_prepare(const char *path, int window)
{
    char why[200];
    size_t n = 0;
    struct stat st;
    if (stat(path, &st) != 0 && errno == ENOENT)
        return PC_APP_SAVES_NEW;   /* si.c makes a new one */
    int kind = read_file(path, g_img, sizeof g_img, &n, why, sizeof why) != 0
                   ? PC_PAK_IMAGE_DAMAGED
                   : pc_pak_image_check(g_img, n, why, sizeof why);
    if (kind == PC_PAK_IMAGE_N64)
        return PC_APP_SAVES_N64;
    if (kind == PC_PAK_IMAGE_OK)
        return backup(path);
    return damaged(path, why, window);
}
