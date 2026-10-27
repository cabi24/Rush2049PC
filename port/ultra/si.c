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
 * Controller 1's pak is a 32 KiB controller pak backed by a file written shortly after each change (temp file
 * + rename), or a rumble pak, or nothing. The image has the .mpk page layout (v5's pak.cpp), but its multi-byte
 * fields are in the order the natively compiled Pfs C writes them: the ID checksums are sums of host-order u16
 * words, and inode entries hold PFS_EOF and PFS_PAGE_NOT_USED as host-order u16 while page links keep their
 * {bank, page} bytes. So N64-order images (v5's, emulators') are detected and left alone, not read; converting
 * between the two needs the save formats too (docs/port/ULTRA.md). */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_compat.h"
#include "ultra.h"

#define PAK_SIZE 0x8000
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

/* ---------------------------------------------------------------- the .mpk image */

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }

/* The ID block's checksums as __osIdCheckSum computes them: over 14 u16 words, host order (big-endian when
 * be is set, as the N64 wrote them). */
static void id_sums(const uint8_t *id, int be, uint16_t *sum, uint16_t *isum)
{
    *sum = *isum = 0;
    for (int j = 0; j < 28; j += 2) {
        uint16_t w;
        if (be)
            w = be16(id + j);
        else
            memcpy(&w, id + j, 2);
        *sum = (uint16_t)(*sum + w);
        *isum = (uint16_t)(*isum + (uint16_t)~w);
    }
}

static int id_valid(const uint8_t *id, int be)
{
    uint16_t sum, isum, s2, i2;
    id_sums(id, be, &sum, &isum);
    if (be)
        s2 = be16(id + 0x1C), i2 = be16(id + 0x1E);
    else
        memcpy(&s2, id + 0x1C, 2), memcpy(&i2, id + 0x1E, 2);
    return sum == s2 && isum == i2;
}

/* A blank, formatted pak for the native Pfs C: label block, ID block (one bank) and its three copies, an inode
 * table (and backup) with pages 5-127 free, an empty note table. */
static void pak_format(uint8_t *img)
{
    memset(img, 0, PAK_SIZE);
    img[0] = 0x81;
    for (int i = 1; i < 32; i++)
        img[i] = (uint8_t)i;
    uint8_t id[32] = {0};
    uint32_t repaired = 0xFFFFFFFFu, random = 0x135F1A05u;
    uint16_t device = 0x0001, sum, isum;   /* device id bit 0: the ID is valid */
    memcpy(id, &repaired, 4);
    memcpy(id + 4, &random, 4);
    memcpy(id + 0x18, &device, 2);
    id[0x1A] = 1;   /* banks */
    id[0x1B] = 0;   /* version */
    id_sums(id, 0, &sum, &isum);
    memcpy(id + 0x1C, &sum, 2);
    memcpy(id + 0x1E, &isum, 2);
    static const int copies[] = {0x20, 0x60, 0x80, 0xC0};   /* PFS_ID_0AREA..3AREA */
    for (int i = 0; i < 4; i++)
        memcpy(img + copies[i], id, 32);
    uint8_t *inode = img + 0x100;
    for (int page = 5; page < 128; page++) {
        uint16_t unused = 0x0003;   /* PFS_PAGE_NOT_USED */
        memcpy(inode + 2 * page, &unused, 2);
    }
    int bytes = 0;
    for (int i = 10; i < 256; i++)   /* __osSumcalc from the first data page's entry */
        bytes += inode[i];
    inode[1] = (uint8_t)bytes;       /* inode_page[0].inode_t.page */
    memcpy(img + 0x200, inode, 256);
}

static int pak_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        pak_format(g_pak);
        g_pak_dirty = 1;
        pc_log("controller pak %s: new, formatted", path);
        return 0;
    }
    size_t n = fread(g_pak, 1, PAK_SIZE, f);
    fclose(f);
    if (n != PAK_SIZE) {
        pc_log("controller pak %s: %zu bytes, not a 32 KiB .mpk; not used", path, n);
        return -1;
    }
    if (!id_valid(g_pak + 0x20, 0) && id_valid(g_pak + 0x20, 1)) {
        pc_log("controller pak %s is in N64 byte order (v5, an emulator): left unchanged, and a blank pak is used "
               "in memory", path);
        pak_format(g_pak);
        return 1;
    }
    if (pc_verbose)
        pc_log("controller pak %s", path);
    return 0;
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
    size_t len = strlen(g_pak_path) + 5;
    char *tmp = malloc(len);
    snprintf(tmp, len, "%s.tmp", g_pak_path);
    FILE *f = fopen(tmp, "wb");
    if (!f || fwrite(copy, 1, PAK_SIZE, f) != PAK_SIZE || fclose(f) != 0)
        pc_log("controller pak: cannot write %s", tmp);
    else if (pc_rename_replace(tmp, g_pak_path) != 0)
        pc_log("controller pak: cannot replace %s", g_pak_path);
    free(tmp);
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
            pak_format(g_pak);   /* in memory only */
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
