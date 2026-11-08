/* GENERATED ROM-aligned TU — segment 0x1f5b0 (rom/lib_1f5b0)
 * layout map ae263abb1203148d19b634cda2c10e4ccdd9bfaa8069507b2028db9a263d7388.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* Shared list/channel records, hoisted before the first slot that uses them
 * (sound_init_handles, sound_init_priority_lists). Text unchanged. */
#pragma pack(1)
typedef struct SequenceNode { struct SequenceNode *next; struct SequenceNode *previous; u32 key; int value; } SequenceNode;
#pragma pack(0)
typedef struct VoicePriorityLink { u8 previous, next; u16 active; } VoicePriorityLink;

/* PROMOTED 2026-10-05 — sound_init_handles
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern unsigned int sound_handle_counter;
/* On the PC the library's per-voice storage is PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9): the sequence nodes, one a voice, seen here in this file's view; PC_MX_LOOP(32) is the
 * run's voices where the N64 links its 32 (docs/port/AUDIO.md 10). */
#define sound_handle_nodes (*(SequenceNode (*)[PC_MX_VOICE_SLOTS])pc_mx_nodes)
#define PC_MX_LOOP(n) PC_MX_SLOTS
extern SequenceNode *sound_handles_used;
extern SequenceNode *sound_handles_free;
void sound_init_handles(void)
{
    SequenceNode *previous;
    int i;
    sound_handle_counter = 0;
    sound_handles_used = 0;
    sound_handles_free = sound_handle_nodes;
    previous = 0;
    for (i = 0; i < PC_MX_LOOP(32); i++) {
        sound_handle_nodes[i].previous = previous;
        if (previous != 0) previous->next = &sound_handle_nodes[i];
        previous = &sound_handle_nodes[i];
    }
    previous->next = 0;
}

/* PROMOTED 2026-10-04 — sound_find_handle
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* The used/free sequence lists share a 16-byte doubly linked node.
 * Voice records have a 0x1A0 stride; parent, node and reset word occupy
 * previously opaque fields at +0x14, +0x18 and +0x28 respectively.
 * Keep these declarations before any future early list-helper promotion. */
#pragma pack(1)
/* PC: the voice state keeps its N64 layout (macros.c), so the node at +0x18, one of sound_handle_nodes's, is
 * kept as its N64 address, 0x80050A50 + 16 * index: N64P(x) gives the node, N64W(x) the word
 * (docs/port/MUSYX_DATA.md 8.3). */
enum n64p_SequenceNode { n64p_SequenceNode_ };
typedef struct VoiceState { u32 command00; u8 unknown04[12]; u32 next_identifier; u32 parent_identifier; enum n64p_SequenceNode entry18; u8 unknown1C[8]; u32 flags24; u32 value28; u8 unknown2C[2]; u8 channel2E; u8 unknown2F[29]; u8 external4C; u8 unknown4D[19]; u32 identifier60; u8 unknown64[89]; u8 activeBD; u8 unknownBE[226]; } VoiceState;
#pragma pack(0)
static SequenceNode *pc_voice_node(enum n64p_SequenceNode address)
{
    return address != 0 ? &sound_handle_nodes[(address - 0x80050A50U) / 16] : 0;
}
static enum n64p_SequenceNode pc_voice_node_address(SequenceNode *node)
{
    return (enum n64p_SequenceNode)(node != 0 ? 0x80050A50U + (u32)(node - sound_handle_nodes) * 16 : 0);
}
#define N64P(x) pc_voice_node(x)
#define N64W(x) pc_voice_node_address(x)
extern SequenceNode *sound_handles_used;
#define sound_synth_voices (*(VoiceState (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)   /* the voices (pc_game_types.h) */
SequenceNode *sound_find_handle(u32 key)
{
    SequenceNode *node;
    node = sound_handles_used;
    while (node != 0) {
        if (key == node->key) return node;
        if (key < node->key) break;
        node = node->next;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_next_handle_key
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int sound_handle_counter;
unsigned int sound_next_handle_key(void)
{
    unsigned int value;
    do { value = sound_handle_counter++; } while (value == 0xFFFFFFFF);
    return value;
}

/* PROMOTED 2026-10-05 — sound_unlink_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern SequenceNode *sound_handles_used, *sound_handles_free;
extern void sound_synth_release_channel(VoiceState *);
void sound_unlink_voice(VoiceState *state)
{
    sound_synth_release_channel(state);
    if (state->identifier60 != 0xFFFFFFFFU) {
        if (state->parent_identifier != 0xFFFFFFFFU) {
            sound_synth_voices[(state->parent_identifier & 255)].next_identifier=state->next_identifier;
            if (state->next_identifier != 0xFFFFFFFFU) {
                sound_synth_voices[(state->next_identifier & 255)].parent_identifier=state->parent_identifier;
            }
        } else if (state->next_identifier != 0xFFFFFFFFU) {
            N64P(state->entry18)->value=state->next_identifier;
            sound_synth_voices[(state->next_identifier & 255)].parent_identifier=0xFFFFFFFFU;
            sound_synth_voices[(state->next_identifier & 255)].entry18=state->entry18;
        } else {
            if (N64P(state->entry18)->previous != 0) {
                N64P(state->entry18)->previous->next=N64P(state->entry18)->next;
            } else {
                sound_handles_used=N64P(state->entry18)->next;
            }
            if (N64P(state->entry18)->next != 0) {
                N64P(state->entry18)->next->previous=N64P(state->entry18)->previous;
            }
            N64P(state->entry18)->next=sound_handles_free;
            if (sound_handles_free != 0) sound_handles_free->previous=N64P(state->entry18);
            N64P(state->entry18)->previous=0;
            sound_handles_free=N64P(state->entry18);
        }
    }
}

/* PROMOTED 2026-10-05 — sound_new_handle
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern u32 sound_next_handle_key(void);
u32 sound_new_handle(VoiceState *state)
{
    SequenceNode *current;
    SequenceNode *previous;
    SequenceNode *node;
    u32 key;
    key = sound_next_handle_key();
    previous = 0;
    for (current = sound_handles_used; current != 0; current = current->next) {
        if (key < current->key) break;
        if (key == current->key) key = sound_next_handle_key();
        previous = current;
    }
    node = sound_handles_free;
    if (node == 0) return 0xFFFFFFFFU;
    if ((sound_handles_free = sound_handles_free->next) != 0)
        sound_handles_free->previous = 0;
    if (previous == 0) sound_handles_used = node;
    else previous->next = node;
    node->previous = previous;
    node->next = current;
    if (current != 0) current->previous = node;
    node->key = key;
    node->value = state->identifier60;
    state->entry18 = N64W(node);
    return key;
}

/* PROMOTED 2026-10-04 — sound_resolve_handle
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern SequenceNode *sound_find_handle(u32 key);
int sound_resolve_handle(u32 key)
{
    SequenceNode *node;
    if (key != 0xFFFFFFFF) {
        node = sound_find_handle(key);
        if (node != 0) return node->value;
    }
    return -1;
}

/* PROMOTED 2026-10-05 — sound_init_priority_lists
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_voice_count;
#define sound_voice_priority_links (*(VoicePriorityLink (*)[PC_MX_VOICE_SLOTS])pc_mx_prio_links)   /* pc_game_types.h */
extern u8 sound_priority_first_voice[256];
extern u16 sound_lowest_priority;
void sound_init_priority_lists(void)
{
    u32 i;
    for (i = 0; i < sound_voice_count; i++) {
        sound_voice_priority_links[i].active = 0;
    }
    for (i = 0; i < 256; i++) {
        sound_priority_first_voice[i] = 255;
    }
    sound_lowest_priority = 65535;
}

/* PROMOTED 2026-10-04 — sound_remove_voice_priority
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct SynthVoicePriorityView { u8 unknown00[46]; u8 channel2E; u8 unknown2F[49]; u32 identifier60; } SynthVoicePriorityView;
#pragma pack(0)
typedef struct PriorityLevelLink { u16 next, previous; } PriorityLevelLink;
extern u8 sound_priority_first_voice[256];
extern PriorityLevelLink sound_priority_levels[256];
extern u16 sound_lowest_priority;
void sound_remove_voice_priority(SynthVoicePriorityView *state)
{
    VoicePriorityLink *link;
    PriorityLevelLink *group;
    link = &sound_voice_priority_links[state->identifier60 & 255];
    if (link->active == 1) {
        if (link->previous != 255) sound_voice_priority_links[link->previous].next = link->next;
        else sound_priority_first_voice[state->channel2E] = link->next;
        if (link->next != 255) sound_voice_priority_links[link->next].previous = link->previous;
        else if (link->previous == 255) {
            group = &sound_priority_levels[state->channel2E];
            if (group->previous != 65535) sound_priority_levels[group->previous].next = group->next;
            else sound_lowest_priority = group->next;
            if (group->next != 65535) sound_priority_levels[group->next].previous = group->previous;
        }
        link->active = 0;
    }
}

/* PROMOTED 2026-10-11 — sound_set_voice_priority
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
#pragma pack(0)
extern void sound_remove_voice_priority(SynthVoicePriorityView *);
void sound_set_voice_priority(VoiceState *state, u8 channel)
{
    u16 current;
    u16 previous;
    VoicePriorityLink *link;
    u8 index;
    index = state->identifier60 & 255;
    link = &sound_voice_priority_links[index];
    if (link->active == 1) {
        if (state->channel2E == channel) return;
        sound_remove_voice_priority((SynthVoicePriorityView *)state);
    }
    link->active = 1;
    link->previous = 255;
    if ((link->next = sound_priority_first_voice[channel]) != 255) {
        sound_voice_priority_links[sound_priority_first_voice[channel]].previous = index;
    } else {
        if (sound_lowest_priority != 65535) {
            if (channel >= sound_lowest_priority) {
                for (current = sound_lowest_priority; current != 65535; current = sound_priority_levels[current].next) {
                    if (channel < current) break;
                    previous = current;
                }
                sound_priority_levels[previous].next = channel;
                sound_priority_levels[channel].previous = previous;
                sound_priority_levels[channel].next = current;
                if (sound_priority_levels[channel].next != 65535) sound_priority_levels[current].previous = channel;
            } else {
                sound_priority_levels[channel].next = sound_lowest_priority;
                sound_priority_levels[channel].previous = 65535;
                sound_priority_levels[sound_lowest_priority].previous = channel;
                sound_lowest_priority = channel;
            }
        } else {
            sound_priority_levels[channel].next = 65535;
            sound_priority_levels[channel].previous = 65535;
            sound_lowest_priority = channel;
        }
    }
    sound_priority_first_voice[channel] = index;
    state->channel2E = channel;
}

typedef struct ChannelLink { u8 previous; u8 next; u16 flags; } ChannelLink;
#define sound_voice_free_links (*(ChannelLink (*)[PC_MX_VOICE_SLOTS])pc_mx_free_links)   /* pc_game_types.h */
/* PROMOTED 2026-10-12 — sound_alloc_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
typedef struct SynthVoiceAllocView { u8 unknown00[0xBA]; u16 allocId; u8 unknownBC; u8 block; u8 unknownBE[226]; } SynthVoiceAllocView;
#pragma pack(0)
extern volatile unsigned char sound_voices_locked;
extern u8 sound_voice_count, sound_music_voice_limit, sound_sfx_voice_limit;
extern u8 sound_voice_free_head, sound_voice_free_tail;
extern unsigned char sound_sfx_voices_used, sound_music_voices_used;
#define ALLOC(i) (((SynthVoiceAllocView *)sound_synth_voices)[i])
int sound_alloc_voice(u8 priority, u8 maxVoices, u16 allocId, u8 fxFlag)
{
    s32 i;
    s32 num;
    s32 voice;
    u16 p;
    u32 type_alloc;
    ChannelLink *sfv;

    if (!sound_voices_locked) {
        if (fxFlag) {
            type_alloc = (sound_sfx_voices_used >= (i = sound_sfx_voice_limit) && sound_voice_count > i) ? 1 : 0;
            if (i <= maxVoices) {
                goto _skip_alloc;
            }
            goto _do_alloc;
        } else {
            type_alloc = (sound_music_voices_used >= (i = sound_music_voice_limit) && sound_voice_count > i) ? 1 : 0;
            if (i <= maxVoices) {
                goto _skip_alloc;
            }
        _do_alloc:
            num = 0;
            voice = -1;
            p = sound_lowest_priority;
            while (p != 0xFFFF && priority >= p && voice == -1) {
                for (i = sound_priority_first_voice[p]; i != 0xFF; i = sound_voice_priority_links[i].next) {
                    if (ALLOC(i).block == 0 && allocId == ALLOC(i).allocId) {
                        ++num;
                        if ((!type_alloc || fxFlag == sound_synth_voices[i].external4C) && !(sound_synth_voices[i].flags24 & 2)) {
                            if (voice != -1) {
                                if (sound_synth_voices[i].value28 < sound_synth_voices[voice].value28)
                                    voice = i;
                            } else
                                voice = i;
                        }
                    }
                }
                p = sound_priority_levels[p].next;
            }
        }
        if (num < maxVoices) {
            do {} while (0);
            while (p != 0xFFFF && num < maxVoices) {
                i = sound_priority_first_voice[p];
                while (i != 0xFF) {
                    if (!ALLOC(i).block) {
                        if (allocId == ALLOC(i).allocId) {
                            num++;
                        }
                    }
                    i = sound_voice_priority_links[i].next;
                }
                p = sound_priority_levels[p].next;
            }
            if (num < maxVoices) {
            _skip_alloc:
                voice = -1;
                if (sound_voice_free_head != 0xFF && type_alloc == 0) {
                    voice = sound_voice_free_head;
                } else {
                if (priority < sound_lowest_priority) {
                    pc_snd_count(PC_SND_MX_REFUSED); return -1;
                }
                p = sound_lowest_priority;
                while (p != 0xFFFF && priority >= p && voice == -1) {
                    for (i = sound_priority_first_voice[p]; i != 0xFF; i = sound_voice_priority_links[i].next) {
                        if (ALLOC(i).block == 0 && (!type_alloc || fxFlag == sound_synth_voices[i].external4C) && !(sound_synth_voices[i].flags24 & 2)) {
                            if (voice != -1) {
                                if (sound_synth_voices[voice].value28 > sound_synth_voices[i].value28)
                                    voice = i;
                            } else
                                voice = i;
                        }
                    }
                    p = sound_priority_levels[p].next;
                }
                if (voice == -1) {
                    pc_snd_count(PC_SND_MX_REFUSED); return -1;
                }
                }
                if (sound_synth_voices[voice].channel2E > priority) {
                    goto _fail;
                }
            }
        }
        if (voice == -1) {
            goto _fail;
        }
        if (sound_voice_free_links[voice].flags == 1) {
            sfv = &sound_voice_free_links[voice];
            i = sfv->previous;
            if (i != 0xFF) {
                sound_voice_free_links[i].next = sfv->next;
            } else {
                sound_voice_free_head = sfv->next;
            }
            i = sfv->next;
            if (i != 0xFF) {
                sound_voice_free_links[i].previous = sfv->previous;
            }
            if (voice == sound_voice_free_tail) {
                sound_voice_free_tail = sfv->previous;
            }
            sfv->flags = 0;
        } else if (sound_synth_voices[voice].external4C) {
            sound_sfx_voices_used--; pc_snd_count(PC_SND_MX_STOLEN);
        } else {
            sound_music_voices_used--; pc_snd_count(PC_SND_MX_STOLEN_SONG);
        }
        if (fxFlag) {
            ++sound_sfx_voices_used;
        } else {
            ++sound_music_voices_used;
        }
        return voice;
    }
_fail:
    pc_snd_count(PC_SND_MX_REFUSED); return -1;
}

/* PROMOTED 2026-10-11 — sound_free_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
#pragma pack(0)
extern u8 sound_voice_free_head;
extern u8 sound_voice_free_tail;
extern unsigned char sound_sfx_voices_used, sound_music_voices_used;
extern void sound_remove_voice_priority(SynthVoicePriorityView *);
void sound_free_voice(VoiceState *state)
{
    u32 index;
    ChannelLink *link;
    sound_remove_voice_priority((SynthVoicePriorityView *)state);
    index = state->identifier60 & 255;
    link = &sound_voice_free_links[index];
    state->command00 = 0;
    state->channel2E = 0;
    if (link->flags == 0) {
        link->flags = 1;
        if (sound_voice_free_head != 255) {
            link->previous = sound_voice_free_tail;
            link->next = 255;
            sound_voice_free_links[sound_voice_free_tail].next = index;
        } else {
            link->next = 255;
            link->previous = 255;
            sound_voice_free_head = index;
        }
        sound_voice_free_tail = index;
        if (state->external4C) {
            --sound_sfx_voices_used;
        } else {
            --sound_music_voices_used;
        }
    }
    state->identifier60 = 0xFFFFFFFFU;
}

/* PROMOTED 2026-10-04 — sound_init_free_voices
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_voice_count;
extern u8 sound_voice_free_head;
extern u8 sound_voice_free_tail;
void sound_init_free_voices(void)
{
    u32 i;
    for (i = 0; i < sound_voice_count; i++) {
        sound_voice_free_links[i].previous = i - 1;
        sound_voice_free_links[i].next = i + 1;
        sound_voice_free_links[i].flags = 1;
    }
    sound_voice_free_links[0].previous = 255;
    sound_voice_free_links[sound_voice_count - 1].next = 255;
    sound_voice_free_head = 0;
    sound_voice_free_tail = sound_voice_count - 1;
}

/* PROMOTED 2026-10-04 — sound_init_voice_allocation
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern void sound_init_free_voices(void);
extern void sound_init_priority_lists(void);
extern unsigned char sound_sfx_voices_used, sound_music_voices_used;
void sound_init_voice_allocation(void)
{
    sound_init_free_voices();
    sound_init_priority_lists();
    sound_sfx_voices_used = 0;
    sound_music_voices_used = 0;
}

/* PROMOTED 2026-10-04 — sound_alloc_stream_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern int sound_alloc_voice(u8, u8, u16, u8);
extern void sound_unlink_voice(VoiceState *);
extern u8 sound_hw_is_voice_active(int);
extern void sound_hw_break_voice(int);
extern void sound_set_voice_priority(VoiceState *, u8);
int sound_alloc_stream_voice(u8 channel)
{
    int index;
    VoiceState *state;
    index = sound_alloc_voice(channel, 255, 65535, 1);
    if (index != -1) {
        state = &sound_synth_voices[index];
        state->activeBD = 1;
        state->external4C = 1;
        sound_unlink_voice(state);
        state->identifier60 = (u32)index | 0xFFFFFF00U;
        if (sound_hw_is_voice_active(index)) sound_hw_break_voice(index);
        state->command00 = 0;
        sound_set_voice_priority(state, channel);
    }
    return index;
}

/* PROMOTED 2026-10-11 — sound_free_stream_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern void sound_free_voice(VoiceState *);
void sound_free_stream_voice(u32 index)
{
    if (index == 0xFFFFFFFFU) {
        return;
    }
    if (sound_hw_is_voice_active(index)) {
        sound_hw_break_voice(index);
    }
    sound_synth_voices[index].identifier60 = index;
    sound_free_voice(&sound_synth_voices[index]);
    sound_synth_voices[index].activeBD = 0;
}

/* PROMOTED 2026-10-05 — sound_end_voice
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern void sound_free_voice(VoiceState *);
void sound_end_voice(VoiceState *state)
{
    sound_unlink_voice(state);
    state->flags24 &= ~3U;
    state->value28 = 0;
    sound_free_voice(state);
}

/* PROMOTED 2026-10-04 — sound_stop_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern u8 sound_initialized;
extern int sound_resolve_handle(u32);
extern void sound_end_voice(VoiceState *);
extern void sound_hw_key_off(int);
int sound_stop_unlocked(u32 key)
{
    int result;
    u32 index;
    u32 next;
    result = -1;
    if (sound_initialized) {
        key = sound_resolve_handle(key);
        while (key != 0xFFFFFFFFU) {
            index = key & 255;
            next = sound_synth_voices[index].next_identifier;
            if (key == sound_synth_voices[index].identifier60) {
                result = 0;
                if (sound_synth_voices[index].command00 != 0) {
                    sound_end_voice(&sound_synth_voices[index]);
                }
                sound_hw_key_off(index);
            }
            key = next;
        }
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_stop_all_voices
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
void sound_stop_all_voices(u8 preserve_external)
{
    int i;
    for (i = 0; i < sound_voice_count; i++) {
        if (sound_synth_voices[i].command00 != 0) {
            if (!preserve_external ||
                (preserve_external && !sound_synth_voices[i].external4C)) {
                sound_end_voice(&sound_synth_voices[i]);
                sound_synth_voices[i].command00 = 0;
                sound_synth_voices[i].flags24 &= ~3U;
            } else {
                continue;
            }
        }
        sound_hw_key_off(i);
    }
}

