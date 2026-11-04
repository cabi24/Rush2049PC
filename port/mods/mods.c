/* port/mods/mods.c: the mods folder (mods.h, docs/port/MODS.md): reading the mods, settling which file each wave
 * takes, the text overrides, the memory, and the file table's entries.
 *
 * Everything here runs at start, before the game's threads: nothing is shared. */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <zlib.h>

#include "assets.h"
#include "config.h"
#include "mods.h"
#include "pc_compat.h"   /* mkdir on Windows */
#include "ultra.h"

#define LOG(...) fprintf(stderr, "rush2049pc: mods: " __VA_ARGS__)

/* The file table's symbols (docs/port/ASSETS.md 2.2): gWaveSizes and the first text bank in the boot image, the ROM
 * offsets and storage kinds in the game image. */
#define SYM_SIZES 0x8002E580u
#define SYM_TEXT_INITIAL 0x8002E860u
#define SYM_OFFSETS 0x8011B5BCu
#define SYM_KINDS 0x80123564u

#define FILE_MAX ((size_t)256 << 20)    /* a mod's file: more is no game file */
#define WAVE_SAMPLES 9
#define WAVE_SDIR 8
#define MiB (1u << 20)

/* ---------------------------------------------------------------- the waves' names */

static const char *const k_text[6] = {"text_en", "text_es", "text_it", "text_de", "text_nl", "text_fr"};
static const char *const k_lang[6] = {"en", "es", "it", "de", "nl", "fr"};
static const char *const k_frontend[7] = {"attract", "interface", "selcar", "selcirc", "selcom", "selopt", "seltrk"};
static const char *const k_race[21] = {
    "raceextra", "hud", "hudbattle", "hudcom", "hudlogo", "hudmulti", "hudstunt", "targetsnopak",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,   /* 69-75: no game code loads them */
    "weapons", "wings", "vehicles", "vehiclesnopak", "wheels", "wheelsnopak"};
static char g_names[PC_MODS_WAVES][24];

static void track_name(int t, char *out, size_t n)
{
    if (t < 6)
        snprintf(out, n, "track%d", t + 1);
    else if (t < 14)
        snprintf(out, n, "battle%d", t - 5);
    else if (t < 18)
        snprintf(out, n, "stunt%d", t - 13);
    else
        snprintf(out, n, "obstacle");
}

static void names_init(void)
{
    char t[16];
    if (g_names[0][0])
        return;
    for (int w = 0; w < PC_MODS_WAVES; w++) {
        char *o = g_names[w];
        size_t n = sizeof g_names[w];
        if (w <= 5)
            snprintf(o, n, "%s", k_text[w]);
        else if (w == 6 || w == 7 || w == 8 || w == 9)
            snprintf(o, n, "%s", w == 6 ? "project" : w == 7 ? "pool" : w == 8 ? "sampledir" : "samples");
        else if (w <= 21)
            snprintf(o, n, "song%02d", w - 9);
        else if (w <= 37)
            snprintf(o, n, "fonttex%02d", w - 22);
        else if (w <= 53)
            snprintf(o, n, "fontdat%02d", w - 38);
        else if (w <= 60)
            snprintf(o, n, "%s", k_frontend[w - 54]);
        else if (w <= 81)
            snprintf(o, n, "%s", k_race[w - 61] ? k_race[w - 61] : "");
        else if (w <= 87)
            snprintf(o, n, "targetst%d", w - 81);
        else if (w <= 100)
            snprintf(o, n, "car%02d", w - 87);
        else if (w <= 119)
            track_name(w - 101, o, n);
        else if (w <= 157) {
            track_name((w - 120) % 19, t, sizeof t);
            snprintf(o, n, "%s_%s", t, w <= 138 ? "world" : "road");
        } else if (w <= 176) {
            track_name(w - 158, t, sizeof t);
            snprintf(o, n, "%s_path", t);
        } else {
            track_name(w - 177, t, sizeof t);
            snprintf(o, n, "%s_pathback", t);
        }
    }
}

const char *pc_mods_wave_name(int wave, const char **folder)
{
    static const char *const folders[] = {"text", "sound", "fonts", "frontend", "race", "cars", "tracks"};
    names_init();
    if (wave < 0 || wave >= PC_MODS_WAVES)
        return NULL;
    if (folder)
        *folder = folders[wave <= 5 ? 0 : wave <= 21 ? 1 : wave <= 53 ? 2 : wave <= 60 ? 3 : wave <= 87 ? 4
                          : wave <= 100 ? 5 : 6];
    return g_names[wave][0] ? g_names[wave] : NULL;
}

int pc_mods_wave_by_name(const char *name)
{
    names_init();
    if (!strncasecmp(name, "wave_", 5) || !strncasecmp(name, "wave", 4)) {
        const char *d = name + (name[4] == '_' ? 5 : 4);
        char *end;
        long w = strtol(d, &end, 10);
        if (isdigit((unsigned char)*d) && !*end && w >= 0 && w < PC_MODS_WAVES)
            return (int)w;
        return -1;
    }
    for (int w = 0; w < PC_MODS_WAVES; w++)
        if (g_names[w][0] && !strcasecmp(name, g_names[w]))
            return w;
    return -1;
}

static int wave_loaded(int wave) { return wave < 69 || wave > 75; }

static void wave_label(int wave, char *out, size_t n)
{
    const char *name = pc_mods_wave_name(wave, NULL);
    snprintf(out, n, name ? "wave %d, %s" : "wave %d", wave, name);
}

/* ---------------------------------------------------------------- decompressors */

/* A growing output buffer. */
typedef struct {
    uint8_t *p;
    size_t n, cap, max;
} Out;

static int out_room(Out *o, size_t more)
{
    if (o->n + more <= o->cap)
        return 1;
    if (o->n + more > o->max)
        return 0;
    size_t cap = o->cap ? o->cap * 2 : 65536;
    while (cap < o->n + more)
        cap *= 2;
    if (cap > o->max)
        cap = o->max;
    uint8_t *p = realloc(o->p, cap);
    if (!p)
        return 0;
    o->p = p, o->cap = cap;
    return 1;
}

/* The boot LZSS (pi.c's lzss_decode): a flag byte, low bit first; 1 a literal, 0 a pair b0 b1 copying (b0 & 0xF) + 2
 * bytes from ((b0 & 0xF0) << 4 | b1) back; 0 0 ends it. A copy from before the start is a bad stream here. */
uint8_t *pc_mods_lzss(const uint8_t *in, size_t n, size_t cap, size_t *out_size)
{
    const uint8_t *end = in + n;
    Out o = {NULL, 0, 0, cap};
    unsigned flags = 0, bits = 0;
    for (;;) {
        if (!bits) {
            if (in >= end)
                break;
            flags = *in++;
            bits = 8;
        }
        int literal = flags & 1;
        flags >>= 1;
        bits--;
        if (literal) {
            if (in >= end)
                break;
            if (!out_room(&o, 1))
                goto fail;
            o.p[o.n++] = *in++;
            continue;
        }
        if (in + 2 > end)
            break;
        unsigned b0 = in[0], b1 = in[1], dist = ((b0 & 0xF0) << 4 | b1) & 0xFFF, len = (b0 & 0xF) + 2;
        in += 2;
        if (dist == 0 && (b0 & 0xF) == 0)
            break;
        if (dist == 0 || dist > o.n || !out_room(&o, len))
            goto fail;
        for (unsigned i = 0; i < len; i++, o.n++)
            o.p[o.n] = o.p[o.n - dist];
    }
    if (!o.p && !(o.p = malloc(1)))
        return NULL;
    *out_size = o.n;
    return o.p;
fail:
    free(o.p);
    return NULL;
}

/* Raw DEFLATE (no zlib header), as inflate_entry reads the cartridge's. */
uint8_t *pc_mods_inflate(const uint8_t *in, size_t n, size_t cap, size_t *out_size)
{
    z_stream z;
    Out o = {NULL, 0, 0, cap};
    int r = Z_OK;
    memset(&z, 0, sizeof z);
    if (inflateInit2(&z, -15) != Z_OK)
        return NULL;
    z.next_in = (Bytef *)in;
    z.avail_in = (uInt)n;
    while (r == Z_OK) {
        if (!out_room(&o, o.cap ? o.cap : 65536) && !out_room(&o, 1))
            break;
        z.next_out = o.p + o.n;
        z.avail_out = (uInt)(o.cap - o.n);
        r = inflate(&z, Z_NO_FLUSH);
        o.n = z.total_out;
        if (r == Z_BUF_ERROR && z.avail_in == 0)
            break;
        if (r == Z_BUF_ERROR)
            r = Z_OK;
    }
    inflateEnd(&z);
    if (r != Z_STREAM_END) {
        free(o.p);
        return NULL;
    }
    *out_size = o.n;
    return o.p;
}

/* ---------------------------------------------------------------- the cartridge's file table */

static const uint8_t *image_at(const PcModsImage *img, const uint8_t *bytes, uint32_t addr, size_t len)
{
    if (addr < img->n64_base || addr - img->n64_base > img->size || len > img->size - (addr - img->n64_base))
        return NULL;
    return bytes + (addr - img->n64_base);
}

int pc_mods_cart_read(PcModsCart *c, const uint8_t *rom, size_t rom_size, const PcModsImage *images, int n_images)
{
    const PcModsImage *boot = NULL, *game = NULL;
    uint8_t *inflated = NULL;
    memset(c, 0, sizeof *c);
    c->rom = rom, c->rom_size = rom_size;
    for (int i = 0; i < n_images; i++) {
        if (!strcmp(images[i].name, "boot"))
            boot = &images[i];
        if (!strcmp(images[i].name, "game"))
            game = &images[i];
    }
    if (!boot || !game || boot->deflate || boot->rom_off > rom_size || boot->size > rom_size - boot->rom_off ||
        game->rom_off >= rom_size)
        return -1;
    const uint8_t *bootb = rom + boot->rom_off, *gameb;
    if (game->deflate) {
        size_t n = 0;
        inflated = pc_mods_inflate(rom + game->rom_off, rom_size - game->rom_off, game->size, &n);
        if (!inflated || n != game->size) {
            free(inflated);
            return -1;
        }
        gameb = inflated;
    } else {
        if (game->size > rom_size - game->rom_off)
            return -1;
        gameb = rom + game->rom_off;
    }
    const uint8_t *sz = image_at(boot, bootb, SYM_SIZES, 4 * PC_MODS_WAVES);
    const uint8_t *ti = image_at(boot, bootb, SYM_TEXT_INITIAL, 1);
    const uint8_t *of = image_at(game, gameb, SYM_OFFSETS, 4 * PC_MODS_WAVES);
    const uint8_t *kd = image_at(game, gameb, SYM_KINDS, 4 * PC_MODS_WAVES);
    int r = sz && ti && of && kd ? 0 : -1;
    for (int w = 0; !r && w < PC_MODS_WAVES; w++) {
        c->size[w] = pc_be32(sz + 4 * w);
        c->off[w] = pc_be32(of + 4 * w) & 0x0FFFFFFFu;
        c->kind[w] = (int32_t)pc_be32(kd + 4 * w);
    }
    if (!r)
        c->text_initial = *ti <= 5 ? *ti : 0;
    free(inflated);
    return r;
}

uint8_t *pc_mods_cart_file(const PcModsCart *c, int wave, size_t *size)
{
    uint32_t off, want;
    uint8_t *p;
    size_t n = 0;
    if (!c->rom || wave < 0 || wave >= PC_MODS_WAVES)
        return NULL;
    off = c->off[wave], want = c->size[wave];
    if (off >= c->rom_size || want > FILE_MAX)
        return NULL;
    if (c->kind[wave] == 0) {
        if (want > c->rom_size - off || !(p = malloc(want ? want : 1)))
            return NULL;
        memcpy(p, c->rom + off, want);
        *size = want;
        return p;
    }
    p = c->kind[wave] == 1 ? pc_mods_lzss(c->rom + off, c->rom_size - off, want, &n)
                           : pc_mods_inflate(c->rom + off, c->rom_size - off, want, &n);
    if (p)
        *size = n;
    return p;
}

/* ---------------------------------------------------------------- reading a mod */

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static uint8_t *read_all(const char *path, size_t *size, char *why, size_t why_size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *p = NULL;
    long len;
    if (!f) {
        snprintf(why, why_size, "it can't be opened: %s", strerror(errno));
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        snprintf(why, why_size, "it can't be read");
    } else if ((size_t)len > FILE_MAX) {
        snprintf(why, why_size, "it is %ld MiB, more than any game file", len >> 20);
    } else if (!(p = malloc(len ? (size_t)len : 1)) || fread(p, 1, (size_t)len, f) != (size_t)len) {
        snprintf(why, why_size, "it can't be read: %s", strerror(errno ? errno : EIO));
        free(p);
        p = NULL;
    } else {
        *size = (size_t)len;
    }
    fclose(f);
    return p;
}

/* A folder name usable as a key in rush2049pc.ini's [mods] and in game.mods_order. */
static int good_folder_name(const char *n)
{
    size_t len = strlen(n);
    if (!len || len >= 64 || n[0] == ' ' || n[len - 1] == ' ' || n[0] == '.')
        return 0;
    for (const char *c = n; *c; c++)
        if (*c == '=' || *c == ';' || *c == '#' || *c == '[' || *c == ']' || *c == ',' || *c == '"' ||
            (unsigned char)*c < 0x20)
            return 0;
    return 1;
}

typedef struct {
    PcMod *m;
    int section;    /* [mod] seen */
} IniCtx;

static int mod_ini_entry(void *user, const char *section, const char *key, const char *value, int line)
{
    IniCtx *x = user;
    PcMod *m = x->m;
    if (strcmp(section, "mod")) {
        LOG("%s: mod.ini:%d: [%s] is not [mod]; ignored\n", m->folder, line, section);
        return 0;
    }
    x->section = 1;
    if (!strcmp(key, "name"))
        snprintf(m->name, sizeof m->name, "%s", value);
    else if (!strcmp(key, "author"))
        snprintf(m->author, sizeof m->author, "%s", value);
    else if (!strcmp(key, "version"))
        snprintf(m->version, sizeof m->version, "%s", value);
    else if (!strcmp(key, "description"))
        snprintf(m->description, sizeof m->description, "%s", value);
    else if (!strcmp(key, "game"))
        snprintf(m->game, sizeof m->game, "%s", value);
    else if (!strcmp(key, "priority")) {
        char *end;
        long p = strtol(value, &end, 10);
        if (*end || !*value || p < -1000 || p > 1000)
            LOG("%s: mod.ini:%d: priority '%s' is not a number from -1000 to 1000; 0\n", m->folder, line, value);
        else
            m->priority = (int)p;
    } else
        LOG("%s: mod.ini:%d: no setting '%s'; ignored\n", m->folder, line, key);
    return 0;
}

static void mod_ini_bad(void *user, int line, const char *why)
{
    LOG("%s: mod.ini:%d: %s; ignored\n", ((IniCtx *)user)->m->folder, line, why);
}

static void read_mod_ini(PcMod *m, const char *text)
{
    IniCtx x = {m, 0};
    pc_ini_parse(text, mod_ini_entry, mod_ini_bad, &x);
    if (!m->name[0])
        snprintf(m->name, sizeof m->name, "%s", m->folder);
    if (!m->game[0])
        LOG("%s: mod.ini doesn't say which game it is for (game = us); taken as the USA cartridge's\n", m->folder);
    else if (strcasecmp(m->game, "us") && strcasecmp(m->game, "usa") && strcasecmp(m->game, "nrue"))
        snprintf(m->problem, sizeof m->problem, "it is for '%s' (mod.ini's game); this port runs the USA "
                 "cartridge's files (game = us)", m->game);
}

static PcModFile *add_file(PcMod *m)
{
    PcModFile *f = realloc(m->files, (size_t)(m->nfiles + 1) * sizeof *f);
    if (!f)
        return NULL;
    m->files = f;
    f = &m->files[m->nfiles++];
    memset(f, 0, sizeof *f);
    return f;
}

/* Text overrides: text/<language>.txt, [single], [taba] or [tab2b] (or [0], [2], [5]), then index = text. Escapes:
 * \n, \t, \\, \", \xHH (a byte). */
typedef struct {
    PcMod *m;
    int wave;
    const char *file;
} TextCtx;

static int text_section(const char *s)
{
    return !strcmp(s, "single") || !strcmp(s, "0") ? 0 : !strcmp(s, "taba") || !strcmp(s, "2") ? 2
           : !strcmp(s, "tab2b") || !strcmp(s, "5") ? 5 : -1;
}

static int text_entry(void *user, const char *section, const char *key, const char *value, int line)
{
    TextCtx *x = user;
    PcMod *m = x->m;
    int s = text_section(section);
    char *end;
    long idx = strtol(key, &end, 10);
    if (s < 0 || *end || !*key || idx < 0 || idx > 0xFFFFF) {
        LOG("%s: %s:%d: not [single], [taba] or [tab2b] then a string's number = its text; ignored\n", m->folder,
            x->file, line);
        return 0;
    }
    size_t n = strlen(value), len = 0;
    char *b = malloc(n + 1);
    PcModString *st;
    if (!b)
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (value[i] != '\\' || i + 1 == n) {
            b[len++] = value[i];
            continue;
        }
        char e = value[++i];
        if (e == 'n')
            b[len++] = '\n';
        else if (e == 't')
            b[len++] = '\t';
        else if (e == 'x' && i + 2 < n + 1 && isxdigit((unsigned char)value[i + 1]) &&
                 isxdigit((unsigned char)value[i + 2])) {
            char hex[3] = {value[i + 1], value[i + 2], 0};
            b[len++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else
            b[len++] = e;
    }
    if (memchr(b, 0, len)) {
        LOG("%s: %s:%d: a string can't hold \\x00; ignored\n", m->folder, x->file, line);
        free(b);
        return 0;
    }
    if (!(st = realloc(m->strings, (size_t)(m->nstrings + 1) * sizeof *st))) {
        free(b);
        return 0;
    }
    m->strings = st;
    m->strings[m->nstrings++] = (PcModString){x->wave, s, (int)idx, b, len};
    return 0;
}

static void text_bad(void *user, int line, const char *why)
{
    TextCtx *x = user;
    LOG("%s: %s:%d: %s; ignored\n", x->m->folder, x->file, line, why);
}

static void read_text_overrides(PcMod *m, const char *rel, const uint8_t *data, size_t size)
{
    char lang[8] = "";
    const char *base = rel + 5;   /* after "text/" */
    int wave = -1;
    if (strlen(base) == 6 && !strcasecmp(base + 2, ".txt"))
        for (int i = 0; i < 6; i++)
            if (!strncasecmp(base, k_lang[i], 2))
                wave = i, snprintf(lang, sizeof lang, "%s", k_lang[i]);
    if (wave < 0) {
        LOG("%s: %s: text overrides are text/en.txt, es, it, de, nl or fr; ignored\n", m->folder, rel);
        return;
    }
    char *text = malloc(size + 1);
    if (!text)
        return;
    memcpy(text, data, size);
    text[size] = 0;
    TextCtx x = {m, wave, rel};
    pc_ini_parse(text, text_entry, text_bad, &x);
    free(text);
}

static int ends_with(const char *s, const char *tail)
{
    size_t a = strlen(s), b = strlen(tail);
    return a >= b && !strcasecmp(s + a - b, tail);
}

/* One file of a mod, by its path inside the mod ("cars/car01.bin"): a replacement, a text override, part of the
 * texture pack, or something to leave alone. data is NULL when the mod is off (only counted). */
static void take_file(PcMod *m, const char *rel, const uint8_t *data, size_t size, int read)
{
    const char *base = strrchr(rel, '/');
    char name[128];
    int stored = 0;
    base = base ? base + 1 : rel;
    if (!strcasecmp(rel, "mod.ini") || !strncasecmp(rel, "textures/", 9) || base[0] == '.')
        return;
    if (!strncasecmp(rel, "text/", 5) && ends_with(rel, ".txt")) {
        if (read)
            read_text_overrides(m, rel, data, size);
        return;
    }
    if (!strncasecmp(base, "readme", 6) || ends_with(base, ".txt") || ends_with(base, ".md") ||
        ends_with(base, ".ini") || ends_with(base, ".png") || ends_with(base, ".jpg") || ends_with(base, ".html"))
        return;
    snprintf(name, sizeof name, "%s", base);
    char *dot = strrchr(name, '.');
    if (dot) {
        if (!strcasecmp(dot, ".lzs"))
            stored = 1;
        else if (!strcasecmp(dot, ".deflate"))
            stored = 2;
        else if (strcasecmp(dot, ".bin")) {
            if (read)
                LOG("%s: %s: not a kind of file mods use (.bin, .lzs, .deflate); ignored\n", m->folder, rel);
            return;
        }
        *dot = 0;
    }
    int wave = pc_mods_wave_by_name(name);
    if (wave < 0) {
        if (read)
            LOG("%s: %s: '%s' names no game file (wave_NNN, or a name: docs/port/MODS.md); ignored\n", m->folder,
                rel, name);
        return;
    }
    if (!wave_loaded(wave)) {
        if (read)
            LOG("%s: %s: no game code loads wave %d; ignored\n", m->folder, rel, wave);
        return;
    }
    PcModFile *f = add_file(m);
    if (!f)
        return;
    f->wave = wave;
    f->stored = stored;
    snprintf(f->path, sizeof f->path, "%s", rel);
    if (!read)
        return;
    if (stored) {
        f->data = stored == 1 ? pc_mods_lzss(data, size, PC_ROM_MODS_MAX, &f->size)
                              : pc_mods_inflate(data, size, PC_ROM_MODS_MAX, &f->size);
        if (!f->data) {
            f->state = PC_MOD_FILE_REFUSED;
            snprintf(f->why, sizeof f->why, "its %s stream doesn't decompress", stored == 1 ? "LZSS" : "DEFLATE");
        }
    } else if ((f->data = malloc(size ? size : 1)) != NULL) {
        memcpy(f->data, data, size);
        f->size = size;
    }
}

static void walk(PcMod *m, const char *dir, const char *rel, int depth, int read)
{
    DIR *d;
    struct dirent *e;
    char **names = NULL;
    int n = 0;
    if (depth > 6 || !(d = opendir(dir)))
        return;
    while ((e = readdir(d)) != NULL) {
        char **nn;
        if (e->d_name[0] == '.' || !(nn = realloc(names, (size_t)(n + 1) * sizeof *nn)))
            continue;
        names = nn;
        names[n++] = strdup(e->d_name);
    }
    closedir(d);
    for (int i = 0; i < n; i++)   /* in name order, so a mod's files are taken the same way everywhere */
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[j], names[i]) < 0) {
                char *t = names[i];
                names[i] = names[j];
                names[j] = t;
            }
    for (int i = 0; i < n; i++) {
        char path[PC_MODS_PATH], sub[512];
        if (!names[i])
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", names[i]);
        if (is_dir(path)) {
            if (!*rel && !strcasecmp(names[i], "textures"))
                snprintf(m->textures, sizeof m->textures, "%s", path);
            else
                walk(m, path, sub, depth + 1, read);
        } else if (!read) {
            take_file(m, sub, NULL, 0, 0);
        } else {
            char why[PC_MODS_WHY];
            size_t size = 0;
            uint8_t *data = NULL;
            int interesting = !(!strcasecmp(sub, "mod.ini") || !strncasecmp(sub, "textures/", 9));
            if (interesting && !(data = read_all(path, &size, why, sizeof why))) {
                LOG("%s: %s: %s; ignored\n", m->folder, sub, why);
            } else if (interesting) {
                take_file(m, sub, data, size, 1);
            }
            free(data);
        }
        free(names[i]);
    }
    free(names);
}

static int mkdirs(const char *dir)
{
    char p[PC_MODS_PATH];
    snprintf(p, sizeof p, "%s", dir);
    for (char *c = p + 1; *c; c++)
        if (*c == '/' || *c == '\\') {
            char sep = *c;
            *c = 0;
            if (c[-1] != ':' && !is_dir(p) && mkdir(p, 0755) != 0 && errno != EEXIST)
                return -1;
            *c = sep;
        }
    return !is_dir(p) && mkdir(p, 0755) != 0 && errno != EEXIST ? -1 : 0;
}

static void read_zip_mod(PcMod *m, const PcModsConfig *cfg, int read)
{
    PcZipEntry *e = NULL;
    int n = 0, pngs = 0;
    char why[PC_MODS_WHY];
    if (pc_mods_zip_read(m->path, &e, &n, why, sizeof why) != 0) {
        snprintf(m->problem, sizeof m->problem, "the zip can't be read: %s", why);
        return;
    }
    /* A zip whose files are all in one folder (as zipping a mod's folder makes): that folder is the mod's top. */
    const char *slash = n ? strchr(e[0].name, '/') : NULL;
    size_t top = slash ? (size_t)(slash - e[0].name + 1) : 0;
    for (int i = 0; top && i < n; i++)
        if (strncmp(e[i].name, e[0].name, top))
            top = 0;
    int ini = -1;
    for (int i = 0; i < n; i++)
        if (!strcasecmp(e[i].name + top, "mod.ini"))
            ini = i;
    if (ini < 0) {
        snprintf(m->problem, sizeof m->problem, "it has no mod.ini (tools/mods/rushmods.py new makes one)");
    } else {
        char *text = malloc(e[ini].size + 1);
        if (text) {
            memcpy(text, e[ini].data, e[ini].size);
            text[e[ini].size] = 0;
            read_mod_ini(m, text);
            free(text);
        }
    }
    for (int i = 0; i < n; i++) {
        const char *rel = e[i].name + top;
        size_t len = strlen(rel);
        if (!len || rel[len - 1] == '/')
            continue;
        if (!strncasecmp(rel, "textures/", 9) && ends_with(rel, ".png")) {
            pngs++;
            if (read && cfg->cache && *cfg->cache && !m->problem[0]) {   /* unpacked for the texture code */
                char path[PC_MODS_PATH], dir[PC_MODS_PATH];
                snprintf(dir, sizeof dir, "%s/%s/textures", cfg->cache, m->folder);
                snprintf(path, sizeof path, "%s/%s/%s", cfg->cache, m->folder, rel);
                char *cut = strrchr(path, '/');
                *cut = 0;
                FILE *f = mkdirs(path) == 0 ? (*cut = '/', fopen(path, "wb")) : NULL;
                if (f) {
                    fwrite(e[i].data, 1, e[i].size, f);
                    fclose(f);
                    snprintf(m->textures, sizeof m->textures, "%s", dir);
                }
            }
            continue;
        }
        take_file(m, rel, e[i].data, e[i].size, read && !m->problem[0]);
    }
    if (pngs && read && !m->textures[0] && !m->problem[0])
        LOG("%s: its %d textures can't be unpacked (no data folder); unpack the zip into a folder\n", m->folder, pngs);
    pc_mods_zip_free(e, n);
}

static void read_folder_mod(PcMod *m, int read)
{
    char ini[PC_MODS_PATH], why[PC_MODS_WHY];
    size_t size = 0;
    snprintf(ini, sizeof ini, "%s/mod.ini", m->path);
    uint8_t *text = read_all(ini, &size, why, sizeof why);
    if (!text) {
        snprintf(m->problem, sizeof m->problem, "it has no mod.ini (tools/mods/rushmods.py new makes one)");
    } else {
        char *t = realloc(text, size + 1);
        if (t) {
            t[size] = 0;
            read_mod_ini(m, t);
            text = (uint8_t *)t;
        }
        free(text);
    }
    walk(m, m->path, "", 0, read && !m->problem[0]);
}

/* ---------------------------------------------------------------- the order */

static int in_list(const char *list, const char *folder)
{
    size_t n = strlen(folder), at = 0;
    int place = 0;
    if (!list)
        return -1;
    while (list[at]) {
        while (list[at] == ' ' || list[at] == ',')
            at++;
        size_t end = at;
        while (list[end] && list[end] != ',')
            end++;
        size_t e2 = end;
        while (e2 > at && list[e2 - 1] == ' ')
            e2--;
        if (e2 > at && e2 - at == n && !strncasecmp(list + at, folder, n))
            return place;
        if (e2 > at)
            place++;
        at = end;
    }
    return -1;
}

static const PcModsConfig *g_sort_cfg;
static const PcMods *g_sort_mods;

static int order_cmp(const void *a, const void *b)
{
    const PcMod *x = &g_sort_mods->mod[*(const int *)a], *y = &g_sort_mods->mod[*(const int *)b];
    int px = in_list(g_sort_cfg->order, x->folder), py = in_list(g_sort_cfg->order, y->folder);
    if ((px >= 0) != (py >= 0))
        return px >= 0 ? -1 : 1;
    if (px >= 0 && px != py)
        return px - py;
    if (x->priority != y->priority)
        return y->priority - x->priority;
    return strcasecmp(x->folder, y->folder);
}

int pc_mods_scan(PcMods *m, const PcModsConfig *cfg)
{
    DIR *d;
    struct dirent *e;
    memset(m, 0, sizeof *m);
    for (int w = 0; w < PC_MODS_WAVES; w++)
        m->wave_mod[w] = m->wave_file[w] = -1;
    snprintf(m->dir, sizeof m->dir, "%s", cfg->dir ? cfg->dir : "");
    m->enabled = cfg->enabled;
    m->memory = cfg->memory;
    if (!m->dir[0] || !(d = opendir(m->dir)))
        return errno == ENOENT || !m->dir[0] ? 0 : (LOG("%s can't be read: %s\n", m->dir, strerror(errno)), -1);
    char names[PC_MODS_MAX][64];
    int n = 0, skipped = 0;
    while ((e = readdir(d)) != NULL) {
        char path[PC_MODS_PATH];
        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof path, "%s/%s", m->dir, e->d_name);
        if (!is_dir(path) && !ends_with(e->d_name, ".zip"))
            continue;   /* the folder's README.txt, anything else */
        if (!good_folder_name(e->d_name)) {
            LOG("'%s': a mod's folder name can't have = ; # [ ] , or quotes (rush2049pc.ini names it); ignored\n",
                e->d_name);
            continue;
        }
        if (n == PC_MODS_MAX) {
            skipped++;
            continue;
        }
        snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    }
    closedir(d);
    if (skipped)
        LOG("more than %d mods in %s: %d not read\n", PC_MODS_MAX, m->dir, skipped);
    qsort(names, (size_t)n, sizeof names[0], (int (*)(const void *, const void *))strcmp);
    for (int i = 0; i < n; i++) {
        int zip = ends_with(names[i], ".zip"), dup = 0;
        char key[64];
        snprintf(key, sizeof key, "%.*s", (int)strlen(names[i]) - (zip ? 4 : 0), names[i]);   /* a zip: its name */
        for (int j = 0; j < m->n; j++)
            dup |= !strcasecmp(m->mod[j].folder, key);
        if (dup) {
            LOG("%s: a mod named %s is there already; ignored\n", names[i], key);
            continue;
        }
        PcMod *mod = &m->mod[m->n++];
        snprintf(mod->folder, sizeof mod->folder, "%s", key);
        snprintf(mod->path, sizeof mod->path, "%s/%s", m->dir, names[i]);
        mod->zip = zip;
        int on = cfg->is_on ? cfg->is_on(cfg->user, mod->folder) : -1;
        mod->on = on != 0;
        if (mod->zip)
            read_zip_mod(mod, cfg, m->enabled && mod->on);
        else
            read_folder_mod(mod, m->enabled && mod->on);
    }
    for (int i = 0; i < m->n; i++)
        m->order[i] = i;
    g_sort_cfg = cfg, g_sort_mods = m;
    qsort(m->order, (size_t)m->n, sizeof m->order[0], order_cmp);
    for (int i = 0; i < m->n; i++)
        m->mod[m->order[i]].place = i;
    return 0;
}

/* ---------------------------------------------------------------- text overrides */

/* The text bank (big-endian, as read) with the strings replaced: each new string goes at the end, after a NUL, and
 * its offset points there; the tables and the other strings stay. NULL when the bank doesn't parse. */
static uint8_t *text_apply(const uint8_t *bank, size_t size, const PcModString *const *set, int n, size_t *out_size)
{
    static const unsigned char offsets[6] = {1, 0, 1, 0, 0, 1};
    size_t pos = 0, at[6], add = 1;
    uint32_t count[6];
    for (int s = 0; s < 6; s++) {
        if (pos + 4 > size)
            return NULL;
        count[s] = pc_be32(bank + pos);
        at[s] = pos + 4;
        size_t bytes = offsets[s] ? 4 * (size_t)count[s] : 2 * (((size_t)count[s] + 1) & ~(size_t)1);
        if (bytes > size - pos - 4)
            return NULL;
        pos += 4 + bytes;
    }
    for (int i = 0; i < n; i++)
        add += set[i]->len + 1;
    uint8_t *out = malloc(size + add);
    if (!out)
        return NULL;
    memcpy(out, bank, size);
    size_t end = size;
    if (end && out[end - 1] != 0)
        out[end++] = 0;
    for (int i = 0; i < n; i++) {
        const PcModString *st = set[i];
        if ((uint32_t)st->index >= count[st->section])
            continue;
        size_t slot = at[st->section] + 4 * (size_t)st->index;
        out[slot] = (uint8_t)(end >> 24), out[slot + 1] = (uint8_t)(end >> 16);
        out[slot + 2] = (uint8_t)(end >> 8), out[slot + 3] = (uint8_t)end;
        memcpy(out + end, st->bytes, st->len);
        end += st->len;
        out[end++] = 0;
    }
    *out_size = end;
    return out;
}

static uint32_t text_count(const uint8_t *bank, size_t size, int section)
{
    static const unsigned char offsets[6] = {1, 0, 1, 0, 0, 1};
    size_t pos = 0;
    for (int s = 0; s < 6 && pos + 4 <= size; s++) {
        uint32_t c = pc_be32(bank + pos);
        if (s == section)
            return c;
        pos += 4 + (offsets[s] ? 4 * (size_t)c : 2 * (((size_t)c + 1) & ~(size_t)1));
    }
    return 0;
}

/* ---------------------------------------------------------------- settling */

static const char *mod_title(const PcMod *mod)
{
    return mod->name[0] ? mod->name : mod->folder;
}

static size_t block(size_t n) { return (n + 31) & ~(size_t)31; }

/* The most more heap the files need than the cartridge's, at once (docs/port/MODS.md 5): the waves the game holds one
 * at a time (a text bank, a song, a track's geometry, world, road and path) count their largest; a car file counts
 * for each car of a race (LoadCars loads each car's own copy); everything else adds up. */
static uint64_t growth(const PcMods *m, const PcModsCart *cart, int cars)
{
    static const int one_at_a_time[][2] = {{10, 21}, {101, 119}, {120, 138}, {139, 157}, {158, 182}};
    uint64_t g[PC_MODS_WAVES] = {0}, total = 0, car = 0;
    size_t text = 0;
    for (int w = 0; w < PC_MODS_WAVES; w++)
        if (m->wave_data[w] && w != WAVE_SAMPLES && w > 5 && block(m->wave_size[w]) > block(cart->size[w]))
            g[w] = block(m->wave_size[w]) - block(cart->size[w]);
    int any_text = 0;
    for (int w = 0; w <= 5; w++) {   /* one block, the first bank's, holds every language (pc_mods_install) */
        size_t n = m->wave_data[w] ? m->wave_size[w] : cart->size[w];
        any_text |= m->wave_data[w] != NULL;
        text = n > text ? n : text;
    }
    if (any_text && block(text) > block(cart->size[cart->text_initial]))
        total += block(text) - block(cart->size[cart->text_initial]);
    for (int w = 88; w <= 100; w++)
        car = g[w] > car ? g[w] : car, g[w] = 0;
    total += car * (uint64_t)cars;
    for (size_t k = 0; k < sizeof one_at_a_time / sizeof *one_at_a_time; k++) {
        uint64_t most = 0;
        for (int w = one_at_a_time[k][0]; w <= one_at_a_time[k][1]; w++)
            most = g[w] > most ? g[w] : most, g[w] = 0;
        total += most;
    }
    for (int w = 0; w < PC_MODS_WAVES; w++)
        total += g[w];
    return total;
}

static void drop_wave(PcMods *m, int w, const char *why)
{
    char label[48];
    wave_label(w, label, sizeof label);
    if (m->wave_mod[w] >= 0) {
        PcMod *mod = &m->mod[m->wave_mod[w]];
        PcModFile *f = &mod->files[m->wave_file[w]];
        f->state = PC_MOD_FILE_REFUSED;
        snprintf(f->why, sizeof f->why, "%s", why);
        mod->used--, mod->refused++;
        LOG("%s: %s (%s) refused: %s; the cartridge's file is used\n", mod_title(mod), f->path, label, why);
    } else {
        LOG("%s: the text overrides refused: %s\n", label, why);
    }
    free(m->made[w]);
    m->made[w] = NULL;
    m->wave_data[w] = NULL;
    m->wave_size[w] = 0;
    m->wave_mod[w] = m->wave_file[w] = -1;
}

/* game.memory for the files (docs/port/MEMORY.md 5): any file larger than the cartridge's needs the heap whole past
 * the runtime images' block (16 MiB or more: at the N64's 8 MiB the free space is split there, and LoadCars' check of
 * the free total passes while no block fits), and 8 MiB plus what the files add. */
static const char *bytes_text(uint64_t n, char *out, size_t size)
{
    if (n >= 100000)
        snprintf(out, size, "%.1f MB", n / 1e6);
    else
        snprintf(out, size, "%.1f KB", n / 1e3);
    return out;
}

static void plan_memory(PcMods *m, const PcModsConfig *cfg, const PcModsCart *cart)
{
    char amount[32];
    int cars = cfg->cars ? cfg->cars : 6;
    uint64_t g = growth(m, cart, cars);
    int have = cfg->memory ? cfg->memory : cfg->expansion_pak ? 8 : 4;
    m->memory = cfg->memory;
    m->growth = g;
    if (!g)
        return;
    uint64_t need = (8ull * MiB + g + MiB - 1) / MiB;
    int size = 16;
    while (size < 256 && (uint64_t)size < need)
        size *= 2;
    if (!cfg->memory && !cfg->expansion_pak) {
        for (int w = 0; w < PC_MODS_WAVES; w++)
            if (m->wave_data[w] && w != WAVE_SAMPLES && block(m->wave_size[w]) > block(cart->size[w]))
                drop_wave(m, w, "it is larger than the cartridge's, which needs game.memory 16 or more, and with "
                          "game.expansion_pak = off that would give the game the Expansion Pak's features");
        m->growth = 0;
        return;
    }
    while (need > 256) {   /* the most game.memory gives: drop the last mod's larger files first */
        int dropped = 0;
        for (int i = m->n - 1; i >= 0 && !dropped; i--)
            for (int w = 0; w < PC_MODS_WAVES && !dropped; w++)
                if (m->wave_mod[w] == m->order[i] && w != WAVE_SAMPLES && block(m->wave_size[w]) > block(cart->size[w])) {
                    drop_wave(m, w, "the mods' larger files need more than 256 MiB, the most game.memory gives");
                    dropped = 1;
                }
        if (!dropped)
            break;
        g = m->growth = growth(m, cart, cars);
        need = (8ull * MiB + g + MiB - 1) / MiB;
        size = 16;
        while (size < 256 && (uint64_t)size < need)
            size *= 2;
    }
    if (!g)
        return;
    if (have >= size && (cfg->memory || have >= 16)) {
        snprintf(m->memory_note, sizeof m->memory_note, "the files need up to %s more than the cartridge's; "
                 "game.memory = %d MiB has room", bytes_text(g, amount, sizeof amount), have);
        return;
    }
    m->memory = size;
    snprintf(m->memory_note, sizeof m->memory_note, "game.memory raised to %d MiB: the files need up to %s more "
             "heap than the cartridge's (a car file counted for each of a race's %d cars)", size,
             bytes_text(g, amount, sizeof amount), cars);
    LOG("%s\n", m->memory_note);
}

int pc_mods_prepare(PcMods *m, const PcModsConfig *cfg, const PcModsCart *cart, uint32_t sample_base)
{
    char label[48];
    uint8_t *cart_file[PC_MODS_WAVES] = {0};
    size_t cart_size[PC_MODS_WAVES] = {0};
    m->sample_base = sample_base;
    m->text_initial = cart->text_initial;
    if (!m->enabled)
        return 0;
    for (int i = 0; i < m->n; i++) {
        PcMod *mod = &m->mod[i];
        if (mod->on && mod->problem[0])
            LOG("%s (%s) is not used: %s\n", mod_title(mod), mod->folder, mod->problem);
    }
    /* each wave: the files of the mods that are on, in the order; the first that passes wins */
    for (int w = 0; w < PC_MODS_WAVES; w++) {
        int winner = -1;
        wave_label(w, label, sizeof label);
        for (int k = 0; k < m->n; k++) {
            int mi = m->order[k];
            PcMod *mod = &m->mod[mi];
            if (!mod->on || mod->problem[0])
                continue;
            for (int j = 0; j < mod->nfiles; j++) {
                PcModFile *f = &mod->files[j];
                if (f->wave != w || f->state == PC_MOD_FILE_REFUSED) {
                    if (f->wave == w && f->state == PC_MOD_FILE_REFUSED && f->why[0]) {
                        LOG("%s: %s (%s) refused: %s; the cartridge's file is used\n", mod_title(mod), f->path, label,
                            f->why);
                        mod->refused++;
                        f->why[0] = 0;   /* said once */
                    }
                    continue;
                }
                if (winner >= 0) {
                    f->state = m->wave_mod[w] == mi ? PC_MOD_FILE_SPARE : PC_MOD_FILE_BEATEN;
                    if (f->state == PC_MOD_FILE_SPARE)
                        LOG("%s gives %s twice: %s is used, not %s\n", mod_title(mod), label,
                            m->mod[m->wave_mod[w]].files[m->wave_file[w]].path, f->path);
                    else
                        LOG("%s: %s's %s and %s's %s; %s is first in the order: its file is used\n", label,
                            mod_title(&m->mod[m->wave_mod[w]]), m->mod[m->wave_mod[w]].files[m->wave_file[w]].path,
                            mod_title(mod), f->path, mod_title(&m->mod[m->wave_mod[w]]));
                    continue;
                }
                if (w != WAVE_SAMPLES && !cart_file[w])
                    cart_file[w] = pc_mods_cart_file(cart, w, &cart_size[w]);
                char why[PC_MODS_WHY];
                if (pc_assets_check(w, f->data, f->size, cart_file[w], cart_size[w], why, sizeof why) != 0) {
                    f->state = PC_MOD_FILE_REFUSED;
                    snprintf(f->why, sizeof f->why, "%s", why);
                    mod->refused++;
                    LOG("%s: %s (%s) refused: %s; %s\n", mod_title(mod), f->path, label, why,
                        "the next mod's or the cartridge's file is used");
                    continue;
                }
                f->state = PC_MOD_FILE_USED;
                mod->used++;
                winner = j;
                m->wave_mod[w] = mi, m->wave_file[w] = j;
                m->wave_data[w] = f->data, m->wave_size[w] = f->size;
            }
        }
    }
    /* text overrides, over the bank in use (a mod's or the cartridge's): the first mod in the order wins a string */
    for (int w = 0; w <= 5; w++) {
        const PcModString *set[4096];
        int n = 0;
        for (int k = m->n - 1; k >= 0; k--) {   /* the last first, so that an earlier mod's string comes later */
            PcMod *mod = &m->mod[m->order[k]];
            if (!mod->on || mod->problem[0])
                continue;
            for (int s = 0; s < mod->nstrings && n < (int)(sizeof set / sizeof *set); s++)
                if (mod->strings[s].wave == w)
                    set[n++] = &mod->strings[s];
        }
        if (!n)
            continue;
        if (!cart_file[w])
            cart_file[w] = pc_mods_cart_file(cart, w, &cart_size[w]);
        const uint8_t *bank = m->wave_data[w] ? m->wave_data[w] : cart_file[w];
        size_t bank_size = m->wave_data[w] ? m->wave_size[w] : cart_size[w], made_size = 0;
        if (!bank)
            continue;
        for (int i = 0; i < n; i++)
            if ((uint32_t)set[i]->index >= text_count(bank, bank_size, set[i]->section))
                LOG("%s: [%s] %d: the text bank has no string %d there; ignored\n", k_lang[w],
                    set[i]->section == 0 ? "single" : set[i]->section == 2 ? "taba" : "tab2b", set[i]->index,
                    set[i]->index);
        uint8_t *made = text_apply(bank, bank_size, set, n, &made_size);
        char why[PC_MODS_WHY];
        if (!made || pc_assets_check(w, made, made_size, cart_file[w], cart_size[w], why, sizeof why) != 0) {
            LOG("%s: the text overrides can't be applied: %s\n", k_lang[w], made ? why : "the bank doesn't parse");
            free(made);
            continue;
        }
        for (int k = 0; k < m->n; k++) {
            PcMod *mod = &m->mod[k];
            for (int s = 0; s < mod->nstrings; s++)
                if (mod->strings[s].wave == w) {
                    int later = 0;   /* an earlier mod in the order sets the same string */
                    for (int k2 = 0; k2 < m->n; k2++) {
                        const PcMod *o = &m->mod[k2];
                        if (o->place >= mod->place || !o->on || o->problem[0])
                            continue;
                        for (int s2 = 0; s2 < o->nstrings; s2++)
                            later |= o->strings[s2].wave == w && o->strings[s2].section == mod->strings[s].section &&
                                     o->strings[s2].index == mod->strings[s].index;
                    }
                    mod->texts_used += mod->on && !mod->problem[0] && !later;
                }
        }
        m->made[w] = made;
        m->wave_data[w] = made, m->wave_size[w] = made_size;
    }
    /* moved samples: the sample directory's offsets will count from the samples' new place (pc_mods_install) */
    if (m->wave_data[WAVE_SAMPLES] && !m->wave_data[WAVE_SDIR]) {
        if (!cart_file[WAVE_SDIR])
            cart_file[WAVE_SDIR] = pc_mods_cart_file(cart, WAVE_SDIR, &cart_size[WAVE_SDIR]);
        if (cart_file[WAVE_SDIR] && (m->made[WAVE_SDIR] = malloc(cart_size[WAVE_SDIR] ? cart_size[WAVE_SDIR] : 1))) {
            memcpy(m->made[WAVE_SDIR], cart_file[WAVE_SDIR], cart_size[WAVE_SDIR]);
            m->wave_data[WAVE_SDIR] = m->made[WAVE_SDIR], m->wave_size[WAVE_SDIR] = cart_size[WAVE_SDIR];
        } else {
            drop_wave(m, WAVE_SAMPLES, "the cartridge's sample directory can't be read to point it at them");
        }
    }
    for (int w = 0; w < PC_MODS_WAVES; w++)
        free(cart_file[w]);
    plan_memory(m, cfg, cart);
    return 0;
}

/* ---------------------------------------------------------------- installing */

int pc_mods_install(PcMods *m, uint32_t *off, uint32_t *size, int32_t *kind)
{
    size_t total = 16, at = 0, place[PC_MODS_WAVES] = {0}, text_block = 0;
    int ti = m->text_initial, text = 0, changed = 0;
    if (!m->enabled)
        return 0;
    /* The text bank read first gets the block every language is read into (ReplaceWave): with a larger bank, its
     * size in the table is the largest, and a mod's file there has the room after it that the read takes. */
    for (int w = 0; w <= 5; w++) {
        size_t n = m->wave_data[w] ? m->wave_size[w] : size[w];
        text |= m->wave_data[w] != NULL;
        text_block = n > text_block ? n : text_block;
    }
    for (int w = 0; w < PC_MODS_WAVES; w++)
        if (m->wave_data[w])
            total += ((w == ti && text_block > m->wave_size[w] ? text_block : m->wave_size[w]) + 15) & ~(size_t)15;
    if (total == 16)
        return 0;
    if (total > PC_ROM_MODS_MAX) {
        LOG("the files are %zu MiB, more than the cartridge's mods region holds (%u MiB): none is used\n", total >> 20,
            PC_ROM_MODS_MAX >> 20);
        return 0;
    }
    free(m->region);
    if (!(m->region = calloc(1, total)))
        return 0;
    for (int w = 0; w < PC_MODS_WAVES; w++)
        if (m->wave_data[w] && w != WAVE_SDIR) {
            place[w] = at;
            memcpy(m->region + at, m->wave_data[w], m->wave_size[w]);
            at += ((w == ti && text_block > m->wave_size[w] ? text_block : m->wave_size[w]) + 15) & ~(size_t)15;
        }
    if (m->wave_data[WAVE_SDIR]) {   /* last: with the samples moved, its offsets count from their new place */
        uint8_t *d = m->region + at;
        place[WAVE_SDIR] = at;
        memcpy(d, m->wave_data[WAVE_SDIR], m->wave_size[WAVE_SDIR]);
        at += (m->wave_size[WAVE_SDIR] + 15) & ~(size_t)15;
        if (m->wave_data[WAVE_SAMPLES]) {
            uint32_t delta = PC_ROM_MODS_BASE + (uint32_t)place[WAVE_SAMPLES] - m->sample_base;
            /* SampleRecord (include/boot_tail_sample_contract.h): u16 id, u16 references, u32 offset at +4 */
            for (size_t r = 0; r + 0x1C <= m->wave_size[WAVE_SDIR] && pc_be16(d + r) != 0xFFFF; r += 0x1C) {
                uint32_t o = pc_be32(d + r + 4) + delta;
                d[r + 4] = (uint8_t)(o >> 24), d[r + 5] = (uint8_t)(o >> 16);
                d[r + 6] = (uint8_t)(o >> 8), d[r + 7] = (uint8_t)o;
            }
        }
    }
    m->region_size = at;
    pc_rom_set_mods(m->region, m->region_size);
    for (int w = 0; w < PC_MODS_WAVES; w++) {
        if (!m->wave_data[w] || w == WAVE_SAMPLES)   /* the samples: the sample directory reaches them */
            continue;
        off[w] = PC_ROM_MODS_BASE + (uint32_t)place[w];
        size[w] = (uint32_t)m->wave_size[w];
        kind[w] = 0;
        changed++;
    }
    if (text && ti >= 0 && ti <= 5 && size[ti] < text_block)
        size[ti] = (uint32_t)text_block;
    m->installed = changed + (m->wave_data[WAVE_SAMPLES] != NULL);
    return m->installed;
}

void pc_mods_texture_dirs(const PcMods *m, char sep, char *out, size_t out_size)
{
    size_t at = 0;
    out[0] = 0;
    if (!m->enabled)
        return;
    for (int k = 0; k < m->n; k++) {
        const PcMod *mod = &m->mod[m->order[k]];
        if (!mod->on || mod->problem[0] || !mod->textures[0])
            continue;
        int n = snprintf(out + at, out_size - at, "%s%s", at ? (char[]){sep, 0} : "", mod->textures);
        if (n < 0 || (size_t)n >= out_size - at) {
            out[at] = 0;
            return;
        }
        at += (size_t)n;
    }
}

void pc_mods_summary(const PcMods *m, char *out, size_t out_size)
{
    size_t at = 0;
    int on = 0, first = 1;
    out[0] = 0;
    if (!m->enabled) {
        snprintf(out, out_size, "off (game.mods)");
        return;
    }
    for (int k = 0; k < m->n; k++)
        on += m->mod[k].on && !m->mod[k].problem[0];
    if (!on) {
        snprintf(out, out_size, m->n ? "none on (%d found)" : "none", m->n);
        return;
    }
    at = (size_t)snprintf(out, out_size, "%d on:", on);
    for (int k = 0; k < m->n && at < out_size; k++) {
        const PcMod *mod = &m->mod[m->order[k]];
        char what[64];
        if (!mod->on || mod->problem[0])
            continue;
        snprintf(what, sizeof what, "%d file%s%s%s", mod->used, mod->used == 1 ? "" : "s",
                 mod->texts_used ? ", text" : "", mod->textures[0] ? ", textures" : "");
        at += (size_t)snprintf(out + at, out_size - at, "%s %s%s%s (%s)", first ? "" : ",", mod_title(mod),
                               mod->version[0] ? " " : "", mod->version, what);
        first = 0;
    }
}

void pc_mods_free(PcMods *m)
{
    for (int i = 0; i < m->n; i++) {
        PcMod *mod = &m->mod[i];
        for (int j = 0; j < mod->nfiles; j++)
            free(mod->files[j].data);
        free(mod->files);
        for (int s = 0; s < mod->nstrings; s++)
            free(mod->strings[s].bytes);
        free(mod->strings);
    }
    for (int w = 0; w < PC_MODS_WAVES; w++)
        free(m->made[w]);
    pc_rom_set_mods(NULL, 0);
    free(m->region);
    memset(m, 0, sizeof *m);
}
