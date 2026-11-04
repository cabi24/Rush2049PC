/* port/assets/tests/test_mxvoices.c: MusyX's per-voice storage (pc_mx_voice, pc_mx_nodes, pc_mx_prio_links,
 * pc_mx_free_links, pc_mx_fx_ctrl, pc_mx_fx_chan_a, pc_mx_fx_chan_b: port/include/pc_game_types.h,
 * docs/port/DECOUPLE.md 9) through the library's own code with more voices than the N64's 32 ([audio] voices,
 * docs/port/AUDIO.md 10), PC_MX_VOICE_SLOTS (96) declared, every entry a sentinel first.
 * The voice allocator (lib_1f5b0.c: func_8001F13C with its free list func_8001F7EC and priority lists func_8001EF8C,
 * func_8001F6EC) gives 80 effect voices (the effects' quota at 96) out of 96, numbers 0 to 79, each its own; an 81st
 * effect takes one of its own kind (a steal, counted) and never a number past the voices; a song's voice still has
 * its quota; freed, they come back. The sequence nodes (func_8001E9B0, func_8001ECE0) give a key to each of 96
 * voices and none to a 97th; at 32 (original) the 33rd has none, as on the N64. An effect voice is its own channel in
 * set 255 (lib_21210.c): its controllers (func_80020610, func_800206AC, func_80020A04, func_80020DA8) and its two
 * channel bytes (func_80020F4C, func_80020FDC) reach voice 90's entries and no other; a song's channel stays in the
 * N64's tables (D_80050D00).
 * game: src/rom/lib_1f5b0.c src/rom/lib_21210.c
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define SENT 0xEE
#define CHECKF(cond, ...)                                                                           \
    do {                                                                                            \
        if (!(cond)) {                                                                              \
            fprintf(stderr, "%s:%d: check failed: %s: ", __FILE__, __LINE__, #cond);                \
            fprintf(stderr, __VA_ARGS__);                                                           \
            fputc('\n', stderr);                                                                     \
            test_failures++;                                                                        \
        }                                                                                           \
    } while (0)

/* ---------------------------------------------------------------- the storage and the library's other globals */

unsigned char pc_mx_voice[PC_MX_VOICE_SLOTS][0x1A0];
struct pc_mx_node pc_mx_nodes[PC_MX_VOICE_SLOTS];
struct pc_mx_link pc_mx_prio_links[PC_MX_VOICE_SLOTS], pc_mx_free_links[PC_MX_VOICE_SLOTS];
unsigned char pc_mx_fx_ctrl[PC_MX_VOICE_SLOTS][134];
unsigned char pc_mx_fx_chan_a[PC_MX_VOICE_SLOTS], pc_mx_fx_chan_b[PC_MX_VOICE_SLOTS];
unsigned char D_8002C630 = 1;
volatile unsigned char D_8004BE94;
unsigned char D_8004FA18, D_8004FA19, D_8004FA1A, D_800504C0, D_800504C1, D_800504C2, D_800504C3;
unsigned char D_80050548[256];
unsigned short D_80050648[256][2];
unsigned short D_80050A48;
unsigned int D_80050A4C;
void *D_80050C50, *D_80050C54;
unsigned char D_80050D00[8][16][134], D_80050C60[8][16], D_800560C0[8][16];

/* The library's functions the test calls. */
extern int func_8001F13C(unsigned char, unsigned char, unsigned short, unsigned char);
extern void func_8001F864(void);
extern void func_8001E9B0(void);
extern void func_8001EF8C(void *, unsigned char);
extern void func_8001F6EC(void *);
extern unsigned int func_8001ECE0(void *);
extern int func_8001EDF4(unsigned int);
extern void func_80020610(unsigned char, unsigned char, unsigned char, unsigned char);
extern void func_80020820(unsigned char, unsigned char);
extern void func_800206AC(unsigned char, unsigned char, unsigned char, unsigned short);
extern unsigned short func_80020A04(unsigned char, unsigned char, unsigned char);
extern void func_80020DA8(unsigned char, void *, void *);
extern void func_80020F4C(unsigned char, unsigned char, unsigned char);
extern unsigned char func_80020F98(unsigned char, unsigned char);
extern void func_80020FDC(unsigned char, unsigned char, unsigned char);
extern unsigned char func_80021028(unsigned char, unsigned char);

/* What the units call outside themselves. */
static unsigned char driver_active[256];
unsigned char func_8001467C(int i) { return driver_active[i & 255]; }
void func_80014AF0(int i) { driver_active[i & 255] = 0; }
void func_80014B3C(int i) { (void)i; }
void func_80021844(void *v) { (void)v; }
void func_80014594(void) {}
void func_800145DC(void) {}

/* A voice's fields at their N64 offsets (lib_1f5b0.c's VoiceState). */
#define VF(i, off, T) (*(T *)(pc_mx_voice[i] + (off)))
#define V_COMMAND 0x00
#define V_NEXT 0x10
#define V_PARENT 0x14
#define V_FLAGS 0x24
#define V_AGE 0x28
#define V_CHANNEL 0x2E
#define V_EXTERNAL 0x4C
#define V_ID 0x60
#define V_ALLOC 0xBA
#define V_BLOCK 0xBD

/* The voices as the synth's init leaves them, with n of them: free, ids clear, the lists and nodes linked. */
static void setup(int n, int effects, int songs)
{
    memset(pc_mx_voice, SENT, sizeof pc_mx_voice);
    memset(pc_mx_nodes, SENT, sizeof pc_mx_nodes);
    memset(pc_mx_prio_links, SENT, sizeof pc_mx_prio_links);
    memset(pc_mx_free_links, SENT, sizeof pc_mx_free_links);
    for (int i = 0; i < n; i++) {
        memset(pc_mx_voice[i], 0, 0x1A0);
        VF(i, V_ID, unsigned int) = VF(i, V_NEXT, unsigned int) = VF(i, V_PARENT, unsigned int) = 0xFFFFFFFFu;
    }
    memset(driver_active, 0, sizeof driver_active);
    test_mx_voices = n < 32 ? 32 : n;
    D_8004FA18 = (unsigned char)n, D_8004FA1A = (unsigned char)effects, D_8004FA19 = (unsigned char)songs;
    func_8001F864();   /* the free list and the priority lists, D_8004FA18 of them */
    func_8001E9B0();   /* the nodes, PC_MX_SLOTS of them */
}


/* What func_80024988 does with the voice it was given: an id with its number in the low byte, its allocation, its
 * kind, playing (the driver), in the priority list. */
static int start(int fx, unsigned short alloc, unsigned char prio)
{
    int v = func_8001F13C(prio, 255, alloc, (unsigned char)fx);
    if (v < 0)
        return v;
    VF(v, V_EXTERNAL, unsigned char) = (unsigned char)fx;
    VF(v, V_ALLOC, unsigned short) = alloc;
    VF(v, V_FLAGS, unsigned int) = 0;
    VF(v, V_AGE, unsigned int) = 0x75300000u - (unsigned)v;   /* the newest the youngest */
    VF(v, V_COMMAND, unsigned int) = 1;
    VF(v, V_ID, unsigned int) = (unsigned)alloc << 16 | (unsigned)v;
    driver_active[v] = 1;
    func_8001EF8C(pc_mx_voice[v], prio);
    return v;
}

static int sentinels(const void *p, size_t n)
{
    const unsigned char *b = p;
    for (size_t i = 0; i < n; i++)
        if (b[i] != SENT)
            return 0;
    return 1;
}

int main(void)
{
    int got[PC_MX_VOICE_SLOTS], seen[256], i;

    /* 1. 96 voices: 80 for the effects, 16 for the songs. */
    setup(96, 80, 16);
    CHECKF(pc_mx_free_links[95].next == 255 && pc_mx_free_links[0].previous == 255 && D_800504C1 == 95,
          "the free list ends at voice 95 (%d %d)", pc_mx_free_links[95].next, D_800504C1);
    CHECKF(sentinels(&pc_mx_voice[96], 0) && sentinels(pc_mx_free_links + 96, 0), "(no storage past 96)");
    memset(seen, 0, sizeof seen);
    int distinct = 1, past = 0;
    for (i = 0; i < 80; i++) {
        got[i] = start(1, (unsigned short)(0x100 + i), 0x40);
        if (got[i] < 0 || got[i] >= 96 || seen[got[i]]++)
            distinct = 0;
        past += got[i] >= 32;
    }
    CHECKF(distinct, "80 effect voices, each its own, below 96");
    CHECKF(past == 48 && D_800504C2 == 80, "48 of them past the N64's 32 (%d), 80 effects counted (%d)", past,
          D_800504C2);
    unsigned long long stolen = test_snd_count[PC_SND_MX_STOLEN];
    int v81 = start(1, 0x200, 0x40);
    CHECKF(v81 >= 0 && v81 < 96 && VF(v81, V_EXTERNAL, unsigned char) == 1 && D_800504C2 == 80 &&
              test_snd_count[PC_SND_MX_STOLEN] == stolen + 1, "the 81st effect takes an effect voice (%d), counted",
          v81);
    int song = start(0, 0x300, 0x40);
    CHECKF(song >= 0 && song < 96 && !seen[song] && D_800504C3 == 1, "a song's voice from the free ones (%d)", song);
    for (i = 0; i < 96; i++)
        if (VF(i, V_COMMAND, unsigned int)) {
            VF(i, V_ID, unsigned int) = (unsigned)i;
            func_8001F6EC(pc_mx_voice[i]);   /* back to the free list */
        }
    CHECKF(D_800504C2 == 0 && D_800504C3 == 0 && D_800504C0 != 255, "all freed");
    int again = start(1, 0x400, 0x40);
    CHECKF(again >= 0 && again < 96, "and given again (%d)", again);

    /* 2. The nodes: a key for each of 96 voices, none for a 97th; at 32, none for a 33rd, as on the N64. */
    setup(96, 80, 16);
    unsigned int keys = 0, key[PC_MX_VOICE_SLOTS];
    for (i = 0; i < 96; i++) {
        VF(i, V_ID, unsigned int) = (unsigned)i;
        key[i] = func_8001ECE0(pc_mx_voice[i]);
        keys += key[i] != 0xFFFFFFFFu;
    }
    unsigned char spare[0x1A0] = {0};
    CHECKF(keys == 96 && func_8001ECE0(spare) == 0xFFFFFFFFu, "96 keys, then none (%u)", keys);
    CHECKF(func_8001EDF4(key[95]) == 95 && func_8001EDF4(key[40]) == 40, "a key's voice: %d %d",
           func_8001EDF4(key[95]), func_8001EDF4(key[40]));
    setup(32, 16, 16);
    keys = 0;
    for (i = 0; i < 33; i++) {
        VF(i, V_ID, unsigned int) = (unsigned)i;
        keys += func_8001ECE0(pc_mx_voice[i]) != 0xFFFFFFFFu;
    }
    CHECKF(keys == 32, "at 32 voices, 32 keys (%u)", keys);
    CHECKF(sentinels(&pc_mx_nodes[32], sizeof pc_mx_nodes - 32 * sizeof *pc_mx_nodes),
          "and the nodes past 32 untouched");

    /* 3. An effect voice's channel in set 255: voice 90's controllers and channel bytes, no other's. */
    memset(pc_mx_fx_ctrl, SENT, sizeof pc_mx_fx_ctrl);
    memset(pc_mx_fx_chan_a, SENT, sizeof pc_mx_fx_chan_a);
    memset(pc_mx_fx_chan_b, SENT, sizeof pc_mx_fx_chan_b);
    memset(D_80050D00, SENT, sizeof D_80050D00);
    func_80020820(90, 255);   /* the defaults: volume 127, pan 64... */
    CHECKF(pc_mx_fx_ctrl[90][7] == 127 && pc_mx_fx_ctrl[90][10] == 64 && pc_mx_fx_chan_a[90] == 255 &&
              pc_mx_fx_chan_b[90] == 0, "voice 90's defaults (%d %d %d %d)", pc_mx_fx_ctrl[90][7],
          pc_mx_fx_ctrl[90][10], pc_mx_fx_chan_a[90], pc_mx_fx_chan_b[90]);
    func_800206AC(1, 90, 255, 0x1234);   /* modulation, 14 bits */
    CHECKF(func_80020A04(1, 90, 255) == 0x1234, "its modulation read back: 0x%04x", func_80020A04(1, 90, 255));
    VF(33, V_ID, unsigned int) = 33, VF(90, V_ID, unsigned int) = 90;
    memset(pc_mx_fx_ctrl[33], 0, 134);
    func_80020DA8(1, pc_mx_voice[33], pc_mx_voice[90]);
    CHECKF(func_80020A04(1, 33, 255) == 0x1234, "copied to voice 33: 0x%04x", func_80020A04(1, 33, 255));
    func_80020F4C(90, 255, 77);
    func_80020FDC(90, 255, 66);
    CHECKF(func_80020F98(90, 255) == 77 && func_80021028(90, 255) == 66, "its channel bytes");
    CHECKF(sentinels(pc_mx_fx_ctrl[91], sizeof pc_mx_fx_ctrl - 91 * 134) && pc_mx_fx_chan_a[91] == SENT &&
              pc_mx_fx_chan_b[89] == SENT && sentinels(pc_mx_fx_ctrl[34], 56 * 134),
          "nothing past voice 90, nor between 34 and 89");
    CHECKF(sentinels(D_80050D00, sizeof D_80050D00), "the songs' tables untouched");
    func_80020610(7, 5, 3, 100);   /* a song's channel: set 3, channel 5 */
    CHECKF(D_80050D00[3][5][7] == 100 && pc_mx_fx_ctrl[5][7] == SENT, "a song's channel in the N64's table");

    return TEST_DONE();
}
