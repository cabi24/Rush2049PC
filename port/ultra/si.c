/* port/ultra/si.c: the serial interface: PIF commands, pads, the controller paks and the rumble paks
 * (docs/port/ULTRA.md).
 *
 * The game's own libultra C (osContInit, osContStartReadData, __osContRamRead/Write, the whole osPfs file
 * system, osMotorInit) runs unchanged: it builds PIF command blocks in RAM and moves them with
 * __osSiRawStartDma. Here a write fills a 64-byte PIF RAM; a read runs its commands against the emulated
 * devices and copies the results back; each sends the SI event the game waits for. Commands: 0x00/0xFF status,
 * 0x01 read buttons, 0x02/0x03 read/write a 32-byte accessory block (address CRC in, data CRC out, computed with
 * the game's own __osContDataCrc). There is no EEPROM: the game saves only to the controller pak.
 *
 * The block layout is the hardware's, byte for byte, because the game fills it through u8 fields; the one u16
 * field (the buttons in __OSContReadFormat) is read natively by the game, so it is written in host order.
 *
 * Each of the four controllers holds one accessory (pc_si_set_pak): a 32 KiB controller pak backed by a file of its
 * own (controller 1's game.pak_file, the others' game.pak2_file...) written shortly after each change (pakfile.c: a
 * temp file flushed to the disk, then renamed over it; a failed write is tried again); a rumble pak; or nothing.
 * Auto is a rumble pak while the port's controller can rumble (PcPad.rumble), else nothing. A change while the game runs is a hot swap, as a player makes one: the old accessory comes out at
 * once, and the new one goes in only after the game's poll of every port (osContStartQuery, every 30 frames) has
 * found the port empty, so the game's pak code sees the old one leave before the new one arrives.
 *
 * The image has the .mpk page layout (v5's pak.cpp), but its multi-byte fields are in the order the natively
 * compiled Pfs C writes them: the ID checksums are sums of host-order u16 words, and inode entries hold PFS_EOF and
 * PFS_PAGE_NOT_USED as host-order u16 while page links keep their {bank, page} bytes. So N64-order images (v5's,
 * emulators') are detected and left alone, not read; converting between the two needs the save formats too
 * (docs/port/ULTRA.md). Each port formats a new pak with an ID of its own: the game takes two paks with one ID for
 * one pak moved from port to port. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_compat.h"
#include "ultra.h"

#define PAK_SIZE PC_PAK_SIZE
#define CONT_TYPE_NORMAL 0x0005

extern uint8_t __osContDataCrc(uint8_t *data);   /* src/rom/lib_105f0.c */

static pthread_mutex_t g_slock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_pif[64];
static PcPad g_pads[4];

/* A controller's accessory. */
typedef struct {
    int kind;                  /* the setting: PC_PAK_NONE, MEMORY, RUMBLE or AUTO */
    int in;                    /* what the game finds plugged in: PC_PAK_NONE, MEMORY or RUMBLE */
    int seen_empty;            /* the game's poll found the port empty since an accessory last came out */
    int found;                 /* the game has read the accessory in now (logged once) */
    int memory_ok;             /* img is a usable pak: its file read, or formatted */
    char *path;                /* the memory pak's file; NULL: in memory only */
    int dirty;
    int failing;               /* its last write failed (said once; tried again at the next flush) */
    int rumble, rumble_seen;   /* the motor; turned on since the last pc_si_motor */
    uint8_t img[PAK_SIZE];
} Port;
static Port g_port[4];
static const char *const kind_names[] = {"nothing", "the controller pak", "the rumble pak", "auto"};

/* ---------------------------------------------------------------- the .mpk image (pakfile.c) */

/* A new pak's ID of the port's own: the random word of pc_pak_format's ID block (controller 1's as it always was)
 * changed by the port, in the block and its three copies, with its checksums. */
static void own_id(uint8_t *img, int ch)
{
    static const int copies[] = {0x20, 0x60, 0x80, 0xC0};   /* PFS_ID_0AREA..3AREA */
    uint8_t *id = img + copies[0];
    uint32_t random;
    uint16_t sum, isum;
    memcpy(&random, id + 4, 4);
    random ^= (uint32_t)ch * 0x2545F491u;
    memcpy(id + 4, &random, 4);
    pc_pak_id_sums(id, 0, &sum, &isum);
    memcpy(id + 0x1C, &sum, 2);
    memcpy(id + 0x1E, &isum, 2);
    for (int i = 1; i < 4; i++)
        memcpy(img + copies[i], id, 32);
}

static void pak_format(uint8_t *img, int ch)
{
    pc_pak_format(img);
    own_id(img, ch);
}

/* Controller ch's pak from path into its image (which the game can't reach meanwhile): 0 when the file's image is
 * the pak (or a new one, formatted, when there is no file: written at the next flush); 1 for a blank pak in memory,
 * the file left alone (N64 byte order); -1 for no pak at all, the file left alone (pakfile.c's pc_pak_file_load). */
static int pak_load(int ch, const char *path)
{
    char why[200];
    switch (pc_pak_file_load(path, g_port[ch].img, why, sizeof why)) {
    case PC_PAK_LOAD_NEW:
        own_id(g_port[ch].img, ch);
        pthread_mutex_lock(&g_slock);
        g_port[ch].dirty = 1;
        pthread_mutex_unlock(&g_slock);
        pc_log("controller %d's pak %s: new, formatted", ch + 1, path);
        return 0;
    case PC_PAK_LOAD_FILE:
        if (pc_verbose)
            pc_log("controller %d's pak %s", ch + 1, path);
        return 0;
    case PC_PAK_LOAD_N64:
        own_id(g_port[ch].img, ch);
        pc_log("controller %d's pak %s: %s; left unchanged, and a blank pak is used in memory", ch + 1, path, why);
        return 1;
    default:
        pc_log("controller %d's pak %s: %s; left unchanged, and no pak is plugged in", ch + 1, path, why);
        return -1;
    }
}

/* The main thread's (the main loop, pc_si_set_pak; --stop-at-gfx's exit), so the path stays. A write that fails
 * keeps the pak dirty: it is tried again at the next flush. */
static void flush_port(int ch)
{
    uint8_t copy[PAK_SIZE];
    Port *p = &g_port[ch];
    pthread_mutex_lock(&g_slock);
    int dirty = p->dirty && p->path;
    if (dirty)
        memcpy(copy, p->img, PAK_SIZE);
    p->dirty = 0;
    pthread_mutex_unlock(&g_slock);
    if (!dirty)
        return;
    if (pc_file_write_atomic(p->path, copy, PAK_SIZE) == 0) {
        if (p->failing)
            pc_log("controller %d's pak: %s written", ch + 1, p->path);
        p->failing = 0;
        return;
    }
    if (!p->failing)
        pc_log("controller %d's pak: cannot write %s: %s (the saves are kept in memory and tried again)", ch + 1,
               p->path, strerror(errno));
    p->failing = 1;
    pthread_mutex_lock(&g_slock);
    p->dirty = 1;
    pthread_mutex_unlock(&g_slock);
}

void pc_si_flush(void)
{
    for (int ch = 0; ch < 4; ch++)
        flush_port(ch);
}

/* ---------------------------------------------------------------- the accessories */

/* What goes in the port, by its setting (lock held). */
static int wanted(int ch)
{
    const Port *p = &g_port[ch];
    switch (p->kind) {
    case PC_PAK_MEMORY: return p->memory_ok ? PC_PAK_MEMORY : PC_PAK_NONE;
    case PC_PAK_RUMBLE: return PC_PAK_RUMBLE;
    case PC_PAK_AUTO: return g_pads[ch].connected && g_pads[ch].rumble ? PC_PAK_RUMBLE : PC_PAK_NONE;
    default: return PC_PAK_NONE;
    }
}

/* The port's accessory follows its setting (lock held): the old one out at once, the new one in once the game's
 * poll has found the port empty. */
static void update(int ch)
{
    Port *p = &g_port[ch];
    int want = wanted(ch);
    if (want == p->in)
        return;
    if (p->in != PC_PAK_NONE) {
        if (pc_verbose)
            pc_log("controller %d: %s out", ch + 1, kind_names[p->in]);
        p->in = PC_PAK_NONE;
        p->rumble = p->rumble_seen = 0;
        p->seen_empty = 0;
    }
    if (want != PC_PAK_NONE && p->seen_empty) {
        if (pc_verbose)
            pc_log("controller %d: %s in", ch + 1, kind_names[want]);
        p->in = want;
        p->found = 0;
    }
}

void pc_si_set_pak_file(int ch, const char *path)
{
    if (ch < 0 || ch > 3)
        return;
    Port *p = &g_port[ch];
    if (p->kind == PC_PAK_MEMORY)
        flush_port(ch);
    pthread_mutex_lock(&g_slock);
    free(p->path);
    p->path = path ? strdup(path) : NULL;
    p->memory_ok = 0;   /* read when it is chosen next */
    update(ch);         /* a memory pak in now comes out */
    pthread_mutex_unlock(&g_slock);
}

int pc_si_set_pak(int ch, int kind)
{
    if (ch < 0 || ch > 3 || kind < PC_PAK_NONE || kind > PC_PAK_AUTO)
        return PC_PAK_NONE;
    Port *p = &g_port[ch];
    int was = p->kind;
    if (kind == PC_PAK_MEMORY && !p->memory_ok && p->in != PC_PAK_MEMORY) {   /* the game can't reach img */
        int r = 0;
        if (p->path)
            r = pak_load(ch, p->path);
        else
            pak_format(p->img, ch);   /* in memory only */
        if (r != 0) {
            pthread_mutex_lock(&g_slock);
            free(p->path);
            p->path = NULL;   /* in memory only, or not used */
            pthread_mutex_unlock(&g_slock);
        }
        for (int o = 0; o < 4 && r >= 0; o++)
            if (o != ch && g_port[o].memory_ok && !memcmp(g_port[o].img + 0x20, p->img + 0x20, 32))
                pc_log("controller %d's pak has the ID of controller %d's (a copy of its file?): the game takes the "
                       "two for one pak", ch + 1, o + 1);
        pthread_mutex_lock(&g_slock);
        p->memory_ok = r >= 0;
        pthread_mutex_unlock(&g_slock);
    }
    pthread_mutex_lock(&g_slock);
    p->kind = kind;
    update(ch);
    pthread_mutex_unlock(&g_slock);
    if (was == PC_PAK_MEMORY && kind != PC_PAK_MEMORY)
        flush_port(ch);   /* taken out: its file is up to date */
    return kind == PC_PAK_MEMORY && !p->memory_ok ? PC_PAK_NONE : kind;
}

int pc_si_pak(int ch)
{
    if (ch < 0 || ch > 3)
        return PC_PAK_NONE;
    pthread_mutex_lock(&g_slock);
    int in = g_port[ch].in;
    pthread_mutex_unlock(&g_slock);
    return in;
}

const char *pc_si_pak_path(int ch)
{
    if (ch < 0 || ch > 3)
        return NULL;
    const Port *p = &g_port[ch];
    return p->kind == PC_PAK_MEMORY && p->memory_ok ? p->path : NULL;
}

int pc_si_init(const char *pak_path, int pak_kind)
{
    for (int ch = 0; ch < 4; ch++)
        g_port[ch].seen_empty = 1;   /* nothing in yet: the first accessory goes in at once */
    pc_si_set_pak_file(0, pak_path);
    pc_si_set_pak(0, pak_kind);
    pthread_mutex_lock(&g_slock);
    if (!g_pads[0].connected)
        g_pads[0].connected = 1;   /* until pc_input_poll says otherwise */
    pthread_mutex_unlock(&g_slock);
    return 0;
}

static int g_pads_held;   /* the self-test's pads */

void pc_si_set_pads(const PcPad pads[4])
{
    pthread_mutex_lock(&g_slock);
    if (!g_pads_held)
        memcpy(g_pads, pads, sizeof g_pads);
    for (int ch = 0; ch < 4; ch++)
        update(ch);   /* auto follows the port's controller */
    pthread_mutex_unlock(&g_slock);
}

void pc_si_test_pads(const PcPad pads[4])
{
    pthread_mutex_lock(&g_slock);
    g_pads_held = pads != NULL;
    if (pads)
        memcpy(g_pads, pads, sizeof g_pads);
    for (int ch = 0; ch < 4; ch++)
        update(ch);
    pthread_mutex_unlock(&g_slock);
}

/* ---------------------------------------------------------------- PIF commands (lock held) */

static uint8_t data_crc(uint8_t *data, int present)
{
    uint8_t crc = __osContDataCrc(data);
    return present ? crc : (uint8_t)~crc;   /* no accessory: a CRC that never matches */
}

static void pak_read(int ch, uint16_t addr, uint8_t *out)
{
    Port *p = &g_port[ch];
    if (!p->found && (p->in == PC_PAK_MEMORY ? addr < PAK_SIZE : addr >= 0x8000 && addr < 0x9000)) {
        p->found = 1;   /* the game's Pfs C (osPfsInitPak) or its motor's (osMotorInit) found it: once each time */
        pc_log("controller %d: the game reads %s", ch + 1, kind_names[p->in]);
    }
    switch (p->in) {
    case PC_PAK_MEMORY:
        if (addr < PAK_SIZE)
            memcpy(out, g_port[ch].img + addr, 32);
        else
            memset(out, 0, 32);
        break;
    case PC_PAK_RUMBLE:
        memset(out, addr >= 0x8000 && addr < 0x9000 ? 0x80 : 0x00, 32);
        break;
    default:
        memset(out, 0, 32);
    }
}

static void pak_write(int ch, uint16_t addr, const uint8_t *in)
{
    Port *p = &g_port[ch];
    if (p->in == PC_PAK_MEMORY && addr < PAK_SIZE) {
        if (memcmp(p->img + addr, in, 32) != 0) {
            memcpy(p->img + addr, in, 32);
            p->dirty = 1;
        }
    } else if (p->in == PC_PAK_RUMBLE && addr >= 0xC000) {
        if (p->rumble != (in[0] != 0) && pc_verbose > 1)
            pc_log("rumble %d %s", ch + 1, in[0] ? "on" : "off");
        p->rumble = in[0] != 0;
        p->rumble_seen |= in[0] != 0;
    }
}

int pc_si_motor(int ch)
{
    if (ch < 0 || ch > 3)
        return 0;
    pthread_mutex_lock(&g_slock);
    int on = g_port[ch].rumble || g_port[ch].rumble_seen;
    g_port[ch].rumble_seen = 0;
    pthread_mutex_unlock(&g_slock);
    return on;
}

/* One channel's command: tx bytes at cmd (cmd[0] is the command), rx bytes of reply at res. */
static int run_command(int ch, uint8_t *cmd, int tx, uint8_t *res, int rx)
{
    int pad = ch < 4 && g_pads[ch].connected;
    if (!pad)
        return 0x80;   /* no response */
    switch (cmd[0]) {
    case 0x00:
    case 0xFF:
        if (rx >= 3) {
            res[0] = CONT_TYPE_NORMAL & 0xFF;   /* typeh: the game reads type = typel << 8 | typeh */
            res[1] = CONT_TYPE_NORMAL >> 8;
            res[2] = g_port[ch].in ? 0x01 : 0x02;   /* accessory present / not */
        }
        return 0;
    case 0x01:
        if (rx >= 4) {
            uint16_t b = g_pads[ch].buttons;
            memcpy(res, &b, 2);   /* read natively by the game as u16 */
            res[2] = (uint8_t)g_pads[ch].stick_x;
            res[3] = (uint8_t)g_pads[ch].stick_y;
        }
        return 0;
    case 0x02:
        if (tx >= 3 && rx >= 33) {
            uint16_t addr = (uint16_t)((cmd[1] << 8 | cmd[2]) & ~0x1F);
            pak_read(ch, addr, res);
            res[32] = data_crc(res, g_port[ch].in != PC_PAK_NONE);
        }
        return 0;
    case 0x03:
        if (tx >= 35 && rx >= 1) {
            uint16_t addr = (uint16_t)((cmd[1] << 8 | cmd[2]) & ~0x1F);
            pak_write(ch, addr, cmd + 3);
            res[0] = data_crc(cmd + 3, g_port[ch].in != PC_PAK_NONE);
        }
        return 0;
    default:
        return 0x80;
    }
}

/* Walk the channel blocks: 0x00 skips a channel, 0xFF pads, 0xFE ends, else tx/rx sizes and the command.
 * Bytes 60-63 are the status word, which the game writes as a native u32 (CONT_CMD_EXE). A status command to every
 * port is the game's poll for accessories (osContStartQuery, osContInit; libultra's own status reads ask one port):
 * a port it finds empty, or without its controller, may take the next accessory. */
static void pif_run(void)
{
    int ch = 0, polled = 0;
    for (int i = 0; i < 60 && ch < 5;) {
        uint8_t tx = g_pif[i];
        if (tx == 0xFE)
            break;
        if (tx == 0xFF) {
            i++;
            continue;
        }
        if (tx == 0x00) {
            ch++;
            i++;
            continue;
        }
        int txn = tx & 0x3F, rxn = g_pif[i + 1] & 0x3F;
        if (i + 2 + txn + rxn > 60)
            break;
        uint8_t *cmd = &g_pif[i + 2], *res = cmd + txn;
        int err = run_command(ch, cmd, txn, res, rxn);
        g_pif[i + 1] = (uint8_t)((g_pif[i + 1] & 0x3F) | err);
        if (ch < 4 && txn >= 1 && (cmd[0] == 0x00 || cmd[0] == 0xFF))
            polled |= 1 << ch;
        i += 2 + txn + rxn;
        ch++;
    }
    for (ch = 0; ch < 4 && polled == 0xF; ch++)
        if (g_port[ch].in == PC_PAK_NONE || !g_pads[ch].connected) {
            g_port[ch].seen_empty = 1;
            update(ch);
        }
}

/* ---------------------------------------------------------------- libultra's raw SI */

/* As on the hardware, the PIF runs the commands in its RAM when the RAM is read back, so a game that only
 * reads again (osContStartReadData after the first time) gets fresh buttons. */
int32_t __osSiRawStartDma(int32_t direction, void *dram)
{
    pthread_mutex_lock(&g_slock);
    if (direction == OS_WRITE) {
        memcpy(g_pif, dram, 64);
    } else {
        pif_run();
        memcpy(dram, g_pif, 64);
    }
    pthread_mutex_unlock(&g_slock);
    pc_os_event(OS_EVENT_SI);
    return 0;
}

int32_t __osSiRawReadIo(uint32_t dev, uint32_t *data)
{
    uint32_t off = (dev & 0x7FF) - 0x7C0;
    pthread_mutex_lock(&g_slock);
    *data = off < 64 ? (uint32_t)g_pif[off] << 24 | (uint32_t)g_pif[off + 1] << 16 | (uint32_t)g_pif[off + 2] << 8 |
                           g_pif[off + 3]
                     : 0;
    pthread_mutex_unlock(&g_slock);
    return 0;
}

int32_t __osSiRawWriteIo(uint32_t dev, uint32_t data)
{
    (void)dev, (void)data;
    return 0;
}

int32_t __osSiDeviceBusy(void) { return 0; }

/* __osContRamRead's name for the data CRC (not in the symbol file). */
uint8_t __osPfsDataChecksum(uint8_t *data) { return __osContDataCrc(data); }

/* Controller 1's pak in memory as pakfile.c judges a file (the self-test: what the game's own Pfs C writes is a good
 * image). */
int pc_si_pak_check(char *why, size_t why_size)
{
    static uint8_t copy[PAK_SIZE];
    pthread_mutex_lock(&g_slock);
    memcpy(copy, g_port[0].img, PAK_SIZE);
    pthread_mutex_unlock(&g_slock);
    return pc_pak_image_check(copy, PAK_SIZE, why, why_size);
}
