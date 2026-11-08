/* port/assets/tests/test_mxvoices.c: MusyX's per-voice storage (pc_mx_voice, pc_mx_nodes, pc_mx_prio_links,
 * pc_mx_free_links, pc_mx_fx_ctrl, pc_mx_fx_chan_a, pc_mx_fx_chan_b: port/include/pc_game_types.h,
 * docs/port/DECOUPLE.md 9) through the library's own code with more voices than the N64's 32 ([audio] voices,
 * docs/port/AUDIO.md 10), PC_MX_VOICE_SLOTS (96) declared, every entry a sentinel first.
 * The voice allocator (voices.c: sound_alloc_voice with its free list sound_init_free_voices and priority lists sound_set_voice_priority,
 * sound_free_voice) gives 80 effect voices (the effects' quota at 96) out of 96, numbers 0 to 79, each its own; an 81st
 * effect takes one of its own kind (a steal, counted) and never a number past the voices; a song's voice still has
 * its quota; freed, they come back. The sequence nodes (sound_init_handles, sound_new_handle) give a key to each of 96
 * voices and none to a 97th; at 32 (original) the 33rd has none, as on the N64. An effect voice is its own channel in
 * set 255 (controllers.c): its controllers (sound_midi_set_controller, sound_midi_set_controller14, sound_midi_get_controller14, sound_midi_copy_voice_controller) and its two
 * channel bytes (sound_midi_set_last_key, sound_midi_set_portamento_state) reach voice 90's entries and no other; a song's channel stays in the
 * N64's tables (sound_midi_controllers).
 * game: src/rom/sound/voices.c src/rom/sound/controllers.c
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
unsigned char sound_initialized = 1;
volatile unsigned char sound_voices_locked;
unsigned char sound_voice_count, sound_music_voice_limit, sound_sfx_voice_limit, sound_voice_free_head, sound_voice_free_tail, sound_sfx_voices_used, sound_music_voices_used;
unsigned char sound_priority_first_voice[256];
unsigned short sound_priority_levels[256][2];
unsigned short sound_lowest_priority;
unsigned int sound_handle_counter;
void *sound_handles_used, *sound_handles_free;
unsigned char sound_midi_controllers[8][16][134], sound_midi_last_keys[8][16], sound_midi_portamento[8][16];

/* The library's functions the test calls. */
extern int sound_alloc_voice(unsigned char, unsigned char, unsigned short, unsigned char);
extern void sound_init_voice_allocation(void);
extern void sound_init_handles(void);
extern void sound_set_voice_priority(void *, unsigned char);
extern void sound_free_voice(void *);
extern unsigned int sound_new_handle(void *);
extern int sound_resolve_handle(unsigned int);
extern void sound_midi_set_controller(unsigned char, unsigned char, unsigned char, unsigned char);
extern void sound_midi_reset_channel(unsigned char, unsigned char);
extern void sound_midi_set_controller14(unsigned char, unsigned char, unsigned char, unsigned short);
extern unsigned short sound_midi_get_controller14(unsigned char, unsigned char, unsigned char);
extern void sound_midi_copy_voice_controller(unsigned char, void *, void *);
extern void sound_midi_set_last_key(unsigned char, unsigned char, unsigned char);
extern unsigned char sound_midi_get_last_key(unsigned char, unsigned char);
extern void sound_midi_set_portamento_state(unsigned char, unsigned char, unsigned char);
extern unsigned char sound_midi_get_portamento_state(unsigned char, unsigned char);

/* What the units call outside themselves. */
static unsigned char driver_active[256];
unsigned char sound_hw_is_voice_active(int i) { return driver_active[i & 255]; }
void sound_hw_break_voice(int i) { driver_active[i & 255] = 0; }
void sound_hw_key_off(int i) { (void)i; }
void sound_synth_release_channel(void *v) { (void)v; }
void sound_hw_lock(void) {}
void sound_hw_unlock(void) {}

/* A voice's fields at their N64 offsets (voices.c's VoiceState). */
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
    sound_voice_count = (unsigned char)n, sound_sfx_voice_limit = (unsigned char)effects, sound_music_voice_limit = (unsigned char)songs;
    sound_init_voice_allocation();   /* the free list and the priority lists, sound_voice_count of them */
    sound_init_handles();   /* the nodes, PC_MX_SLOTS of them */
}


/* What sound_macro_start does with the voice it was given: an id with its number in the low byte, its allocation, its
 * kind, playing (the driver), in the priority list. */
static int start(int fx, unsigned short alloc, unsigned char prio)
{
    int v = sound_alloc_voice(prio, 255, alloc, (unsigned char)fx);
    if (v < 0)
        return v;
    VF(v, V_EXTERNAL, unsigned char) = (unsigned char)fx;
    VF(v, V_ALLOC, unsigned short) = alloc;
    VF(v, V_FLAGS, unsigned int) = 0;
    VF(v, V_AGE, unsigned int) = 0x75300000u - (unsigned)v;   /* the newest the youngest */
    VF(v, V_COMMAND, unsigned int) = 1;
    VF(v, V_ID, unsigned int) = (unsigned)alloc << 16 | (unsigned)v;
    driver_active[v] = 1;
    sound_set_voice_priority(pc_mx_voice[v], prio);
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
    CHECKF(pc_mx_free_links[95].next == 255 && pc_mx_free_links[0].previous == 255 && sound_voice_free_tail == 95,
          "the free list ends at voice 95 (%d %d)", pc_mx_free_links[95].next, sound_voice_free_tail);
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
    CHECKF(past == 48 && sound_sfx_voices_used == 80, "48 of them past the N64's 32 (%d), 80 effects counted (%d)", past,
          sound_sfx_voices_used);
    unsigned long long stolen = test_snd_count[PC_SND_MX_STOLEN];
    int v81 = start(1, 0x200, 0x40);
    CHECKF(v81 >= 0 && v81 < 96 && VF(v81, V_EXTERNAL, unsigned char) == 1 && sound_sfx_voices_used == 80 &&
              test_snd_count[PC_SND_MX_STOLEN] == stolen + 1, "the 81st effect takes an effect voice (%d), counted",
          v81);
    int song = start(0, 0x300, 0x40);
    CHECKF(song >= 0 && song < 96 && !seen[song] && sound_music_voices_used == 1 && test_snd_count[PC_SND_MX_STOLEN_SONG] == 0,
           "a song's voice from the free ones (%d)", song);
    for (i = 0; i < 96; i++)
        if (VF(i, V_COMMAND, unsigned int)) {
            VF(i, V_ID, unsigned int) = (unsigned)i;
            sound_free_voice(pc_mx_voice[i]);   /* back to the free list */
        }
    CHECKF(sound_sfx_voices_used == 0 && sound_music_voices_used == 0 && sound_voice_free_head != 255, "all freed");
    int again = start(1, 0x400, 0x40);
    CHECKF(again >= 0 && again < 96, "and given again (%d)", again);

    /* 2. The nodes: a key for each of 96 voices, none for a 97th; at 32, none for a 33rd, as on the N64. */
    setup(96, 80, 16);
    unsigned int keys = 0, key[PC_MX_VOICE_SLOTS];
    for (i = 0; i < 96; i++) {
        VF(i, V_ID, unsigned int) = (unsigned)i;
        key[i] = sound_new_handle(pc_mx_voice[i]);
        keys += key[i] != 0xFFFFFFFFu;
    }
    unsigned char spare[0x1A0] = {0};
    CHECKF(keys == 96 && sound_new_handle(spare) == 0xFFFFFFFFu, "96 keys, then none (%u)", keys);
    CHECKF(sound_resolve_handle(key[95]) == 95 && sound_resolve_handle(key[40]) == 40, "a key's voice: %d %d",
           sound_resolve_handle(key[95]), sound_resolve_handle(key[40]));
    setup(32, 16, 16);
    keys = 0;
    for (i = 0; i < 33; i++) {
        VF(i, V_ID, unsigned int) = (unsigned)i;
        keys += sound_new_handle(pc_mx_voice[i]) != 0xFFFFFFFFu;
    }
    CHECKF(keys == 32, "at 32 voices, 32 keys (%u)", keys);
    CHECKF(sentinels(&pc_mx_nodes[32], sizeof pc_mx_nodes - 32 * sizeof *pc_mx_nodes),
          "and the nodes past 32 untouched");

    /* 3. An effect voice's channel in set 255: voice 90's controllers and channel bytes, no other's. */
    memset(pc_mx_fx_ctrl, SENT, sizeof pc_mx_fx_ctrl);
    memset(pc_mx_fx_chan_a, SENT, sizeof pc_mx_fx_chan_a);
    memset(pc_mx_fx_chan_b, SENT, sizeof pc_mx_fx_chan_b);
    memset(sound_midi_controllers, SENT, sizeof sound_midi_controllers);
    sound_midi_reset_channel(90, 255);   /* the defaults: volume 127, pan 64... */
    CHECKF(pc_mx_fx_ctrl[90][7] == 127 && pc_mx_fx_ctrl[90][10] == 64 && pc_mx_fx_chan_a[90] == 255 &&
              pc_mx_fx_chan_b[90] == 0, "voice 90's defaults (%d %d %d %d)", pc_mx_fx_ctrl[90][7],
          pc_mx_fx_ctrl[90][10], pc_mx_fx_chan_a[90], pc_mx_fx_chan_b[90]);
    sound_midi_set_controller14(1, 90, 255, 0x1234);   /* modulation, 14 bits */
    CHECKF(sound_midi_get_controller14(1, 90, 255) == 0x1234, "its modulation read back: 0x%04x", sound_midi_get_controller14(1, 90, 255));
    VF(33, V_ID, unsigned int) = 33, VF(90, V_ID, unsigned int) = 90;
    memset(pc_mx_fx_ctrl[33], 0, 134);
    sound_midi_copy_voice_controller(1, pc_mx_voice[33], pc_mx_voice[90]);
    CHECKF(sound_midi_get_controller14(1, 33, 255) == 0x1234, "copied to voice 33: 0x%04x", sound_midi_get_controller14(1, 33, 255));
    sound_midi_set_last_key(90, 255, 77);
    sound_midi_set_portamento_state(90, 255, 66);
    CHECKF(sound_midi_get_last_key(90, 255) == 77 && sound_midi_get_portamento_state(90, 255) == 66, "its channel bytes");
    CHECKF(sentinels(pc_mx_fx_ctrl[91], sizeof pc_mx_fx_ctrl - 91 * 134) && pc_mx_fx_chan_a[91] == SENT &&
              pc_mx_fx_chan_b[89] == SENT && sentinels(pc_mx_fx_ctrl[34], 56 * 134),
          "nothing past voice 90, nor between 34 and 89");
    CHECKF(sentinels(sound_midi_controllers, sizeof sound_midi_controllers), "the songs' tables untouched");
    sound_midi_set_controller(7, 5, 3, 100);   /* a song's channel: set 3, channel 5 */
    CHECKF(sound_midi_controllers[3][5][7] == 100 && pc_mx_fx_ctrl[5][7] == SENT, "a song's channel in the N64's table");

    return TEST_DONE();
}
