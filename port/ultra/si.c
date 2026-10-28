/* port/ultra/si.c: the serial interface: PIF commands, pads, the controller pak and the rumble pak
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
 * Controller 1's pak is a 32 KiB controller pak backed by a file written shortly after each change (pakfile.c: a
 * temp file flushed to the disk, then renamed over it), or a rumble pak, or nothing. The image has the .mpk page
 * layout (v5's pak.cpp), but its multi-byte fields are in the order the natively compiled Pfs C writes them: the ID checksums are sums of host-order u16
 * words, and inode entries hold PFS_EOF and PFS_PAGE_NOT_USED as host-order u16 while page links keep their
 * {bank, page} bytes. So N64-order images (v5's, emulators') are detected and left alone, not read; converting
 * between the two needs the save formats too (docs/port/ULTRA.md). */
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
static int g_pak_kind[4];
static uint8_t g_pak[PAK_SIZE];
static char *g_pak_path;
static int g_pak_dirty;
static int g_rumble[4];
static int g_rumble_seen[4];   /* turned on since the last pc_si_motor */

/* ---------------------------------------------------------------- the .mpk image (pakfile.c) */

/* 0: the file's image is the pak (or a new one, formatted, when there is no file); 1: a blank pak in memory, the
 * file left alone (N64 byte order); -1: no pak at all, the file left alone (pakfile.c's pc_pak_file_load). */
static int pak_load(const char *path)
{
    char why[200];
    switch (pc_pak_file_load(path, g_pak, why, sizeof why)) {
    case PC_PAK_LOAD_NEW:
        g_pak_dirty = 1;
        pc_log("controller pak %s: new, formatted", path);
        return 0;
    case PC_PAK_LOAD_FILE:
        if (pc_verbose)
            pc_log("controller pak %s", path);
        return 0;
    case PC_PAK_LOAD_N64:
        pc_log("controller pak %s: %s; left unchanged, and a blank pak is used in memory", path, why);
        return 1;
    default:
        pc_log("controller pak %s: %s; left unchanged, and no pak is plugged in", path, why);
        return -1;
    }
}

void pc_si_flush(void)
{
    uint8_t copy[PAK_SIZE];
    pthread_mutex_lock(&g_slock);
    int dirty = g_pak_dirty && g_pak_path;
    if (dirty)
        memcpy(copy, g_pak, PAK_SIZE);
    g_pak_dirty = 0;
    pthread_mutex_unlock(&g_slock);
    if (!dirty)
        return;
    static int failing;   /* said once; tried again at the next flush */
    if (pc_file_write_atomic(g_pak_path, copy, PAK_SIZE) == 0) {
        if (failing)
            pc_log("controller pak: %s written", g_pak_path);
        failing = 0;
        return;
    }
    if (!failing)
        pc_log("controller pak: cannot write %s: %s (the saves are kept in memory and tried again)", g_pak_path,
               strerror(errno));
    failing = 1;
    pthread_mutex_lock(&g_slock);
    g_pak_dirty = 1;
    pthread_mutex_unlock(&g_slock);
}

int pc_si_init(const char *pak_path, int pak_kind)
{
    g_pak_kind[0] = pak_kind;
    if (pak_kind == PC_PAK_MEMORY) {
        if (pak_path) {
            g_pak_path = strdup(pak_path);
            int r = pak_load(pak_path);
            if (r != 0) {
                free(g_pak_path);
                g_pak_path = NULL;   /* in memory only */
                if (r < 0)
                    g_pak_kind[0] = PC_PAK_NONE;
            }
        } else {
            pc_pak_format(g_pak);   /* in memory only */
        }
    }
    if (!g_pads[0].connected)
        g_pads[0].connected = 1;   /* until pc_input_poll says otherwise */
    return 0;
}

void pc_si_set_pads(const PcPad pads[4])
{
    pthread_mutex_lock(&g_slock);
    memcpy(g_pads, pads, sizeof g_pads);
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
    switch (g_pak_kind[ch]) {
    case PC_PAK_MEMORY:
        if (addr < PAK_SIZE)
            memcpy(out, g_pak + addr, 32);
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
    if (g_pak_kind[ch] == PC_PAK_MEMORY && addr < PAK_SIZE) {
        if (memcmp(g_pak + addr, in, 32) != 0) {
            memcpy(g_pak + addr, in, 32);
            g_pak_dirty = 1;
        }
    } else if (g_pak_kind[ch] == PC_PAK_RUMBLE && addr >= 0xC000) {
        if (g_rumble[ch] != (in[0] != 0) && pc_verbose > 1)
            pc_log("rumble %d %s", ch + 1, in[0] ? "on" : "off");
        g_rumble[ch] = in[0] != 0;
        g_rumble_seen[ch] |= in[0] != 0;
    }
}

int pc_si_motor(int ch)
{
    if (ch < 0 || ch > 3)
        return 0;
    pthread_mutex_lock(&g_slock);
    int on = g_rumble[ch] || g_rumble_seen[ch];
    g_rumble_seen[ch] = 0;
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
            res[2] = g_pak_kind[ch] ? 0x01 : 0x02;   /* accessory present / not */
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
            res[32] = data_crc(res, g_pak_kind[ch] != PC_PAK_NONE);
        }
        return 0;
    case 0x03:
        if (tx >= 35 && rx >= 1) {
            uint16_t addr = (uint16_t)((cmd[1] << 8 | cmd[2]) & ~0x1F);
            pak_write(ch, addr, cmd + 3);
            res[0] = data_crc(cmd + 3, g_pak_kind[ch] != PC_PAK_NONE);
        }
        return 0;
    default:
        return 0x80;
    }
}

/* Walk the channel blocks: 0x00 skips a channel, 0xFF pads, 0xFE ends, else tx/rx sizes and the command.
 * Bytes 60-63 are the status word, which the game writes as a native u32 (CONT_CMD_EXE). */
static void pif_run(void)
{
    int ch = 0;
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
        i += 2 + txn + rxn;
        ch++;
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

/* The pak in memory as pakfile.c judges a file (the self-test: what the game's own Pfs C writes is a good image). */
int pc_si_pak_check(char *why, size_t why_size)
{
    static uint8_t copy[PAK_SIZE];
    pthread_mutex_lock(&g_slock);
    memcpy(copy, g_pak, PAK_SIZE);
    pthread_mutex_unlock(&g_slock);
    return pc_pak_image_check(copy, PAK_SIZE, why, why_size);
}
