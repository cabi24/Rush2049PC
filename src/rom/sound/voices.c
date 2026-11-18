/* src/rom/sound/voices.c: the MusyX sound library's voice allocation and sound handles.
 *
 * A sound plays on one or more synth voices (sound_synth_voices, SYNTH_VOICE_SIZE bytes each), chained by voice id
 * (the head's next and previous). The caller holds a handle: a key in a sorted list of handle nodes
 * (sound_handle_nodes) whose node names the chain's first voice, so a handle stays valid, or turns invalid, when
 * voices are stolen and reused
 * (sound_new_handle, sound_resolve_handle, sound_unlink_voice). A voice id is (macro << 16 | key << 8 | voice index);
 * VOICE_NONE ends a chain and means "no voice".
 *
 * sound_alloc_voice gives a sound a voice: a free one, or by stealing the oldest voice of a priority no higher than the
 * new sound's, within the effects' and the music's voice quotas and the sound's own voice limit. Voices are kept in
 * per-priority lists (sound_priority_first_voice, sound_voice_priority_links), the priorities in use in a sorted list
 * (sound_priority_levels from sound_lowest_priority), the free voices in a list of their own (sound_voice_free_links).
 * A stream voice (streams_3d.c) is allocated here and blocked from the synth (sound_alloc_stream_voice).
 *
 * MusyX is the N64's sound library: no MAT3 counterpart. On the PC the per-voice storage is PC data, PC_MX_VOICE_SLOTS
 * long (pc_game_types.h, docs/port/DECOUPLE.md 9), where the N64 has 32 voices; a voice is musyx.h's SynthVoice. */
#include "../rom_tu.h"
#include "musyx.h"

#define LINK_END 255               /* ends a voice list (priority and free links, sound_priority_first_voice) */
#define LEVEL_END 65535            /* ends the list of priorities in use */
#define ALLOC_ID_NONE 65535        /* the allocation id of a stream voice */

/* A handle node: the handle's key and the id of its chain's first voice. sound_handles_used is sorted by key. */
#pragma pack(1)
typedef struct HandleNode { struct HandleNode *next; struct HandleNode *previous; u32 key; int voice; } HandleNode;
#pragma pack(0)

/* A voice's link in its priority's list. */
typedef struct VoicePriorityLink { u8 previous, next; u16 active; } VoicePriorityLink;

/* On the PC the library's per-voice storage is PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9): the handle nodes, one a voice, seen here in this file's view; PC_MX_LOOP(32) is the
 * run's voices where the N64 links its 32 (docs/port/AUDIO.md 10). */
#define sound_handle_nodes (*(HandleNode (*)[PC_MX_VOICE_SLOTS])pc_mx_nodes)

/* A voice's handle node is its head's node (musyx.h's SynthVoice), one of sound_handle_nodes. */

/* A priority's link in the list of priorities in use. */
typedef struct PriorityLevelLink { u16 next, previous; } PriorityLevelLink;
#define sound_voice_priority_links (*(VoicePriorityLink (*)[PC_MX_VOICE_SLOTS])pc_mx_prio_links)   /* pc_game_types.h */

/* A voice's link in the free list; flags is 1 while the voice is on it. */
typedef struct ChannelLink { u8 previous; u8 next; u16 flags; } ChannelLink;
#define sound_voice_free_links (*(ChannelLink (*)[PC_MX_VOICE_SLOTS])pc_mx_free_links)   /* pc_game_types.h */

/* sound_alloc_voice reads a candidate's blocked flag and allocation id through the voices as a pointer rather than
 * as the array: the same voice, and the loops GCC makes of it as they were. */
#define VOICE_AT(i) (((SynthVoice *)sound_synth_voices)[i])


extern unsigned int sound_handle_counter;
extern HandleNode *sound_handles_used;
extern HandleNode *sound_handles_free;
extern void sound_synth_release_channel(SynthVoice *);
extern u8 sound_voice_count;
extern u8 sound_priority_first_voice[256];
extern u16 sound_lowest_priority;
extern PriorityLevelLink sound_priority_levels[256];
extern volatile unsigned char sound_voices_locked;
extern u8 sound_music_voice_limit, sound_sfx_voice_limit;
extern u8 sound_voice_free_head, sound_voice_free_tail;
extern unsigned char sound_sfx_voices_used, sound_music_voices_used;
extern u8 sound_hw_is_voice_active(int);
extern void sound_hw_break_voice(int);
extern u8 sound_initialized;
extern void sound_hw_key_off(int);

/* Empties the handle list and links every handle node into the free list (the library's start-up). */
void sound_init_handles(void)
{
    HandleNode *previous;
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

/* The used handle node with this key, or NULL (the list is sorted, so the walk stops at a larger key). */
HandleNode *sound_find_handle(u32 key)
{
    HandleNode *node;
    node = sound_handles_used;
    while (node != 0) {
        if (key == node->key) return node;
        if (key < node->key) break;
        node = node->next;
    }
    return 0;
}

/* The next handle key from the counter, skipping VOICE_NONE. */
unsigned int sound_next_handle_key(void)
{
    unsigned int key;
    do { key = sound_handle_counter++; } while (key == VOICE_NONE);
    return key;
}

/* Takes a voice out of its chain before it is reused: gives up its MIDI channel (sound_synth_release_channel), joins
 * its neighbours, makes its successor the chain's first voice if it was, and frees the handle node when it was the
 * chain's only voice. */
void sound_unlink_voice(SynthVoice *state)
{
    sound_synth_release_channel(state);
    if (state->id != VOICE_NONE) {
        if (state->head.previous != VOICE_NONE) {
            sound_synth_voices[(state->head.previous & VOICE_INDEX_MASK)].head.next=state->head.next;
            if (state->head.next != VOICE_NONE) {
                sound_synth_voices[(state->head.next & VOICE_INDEX_MASK)].head.previous=state->head.previous;
            }
        } else if (state->head.next != VOICE_NONE) {
            state->head.node->voice=state->head.next;
            sound_synth_voices[(state->head.next & VOICE_INDEX_MASK)].head.previous=VOICE_NONE;
            sound_synth_voices[(state->head.next & VOICE_INDEX_MASK)].head.node=state->head.node;
        } else {
            if (state->head.node->previous != 0) {
                state->head.node->previous->next=state->head.node->next;
            } else {
                sound_handles_used=state->head.node->next;
            }
            if (state->head.node->next != 0) {
                state->head.node->next->previous=state->head.node->previous;
            }
            state->head.node->next=sound_handles_free;
            if (sound_handles_free != 0) sound_handles_free->previous=state->head.node;
            state->head.node->previous=0;
            sound_handles_free=state->head.node;
        }
    }
}

/* Makes a handle for the chain that starts at this voice: a new key, unique among the used handles, inserted in key
 * order with a free node that names the voice. Returns the key, or VOICE_NONE if no node is free. */
u32 sound_new_handle(SynthVoice *state)
{
    HandleNode *current;
    HandleNode *previous;
    HandleNode *node;
    u32 key;
    key = sound_next_handle_key();
    previous = 0;
    for (current = sound_handles_used; current != 0; current = current->next) {
        if (key < current->key) break;
        if (key == current->key) key = sound_next_handle_key();
        previous = current;
    }
    node = sound_handles_free;
    if (node == 0) return VOICE_NONE;
    if ((sound_handles_free = sound_handles_free->next) != 0)
        sound_handles_free->previous = 0;
    if (previous == 0) sound_handles_used = node;
    else previous->next = node;
    node->previous = previous;
    node->next = current;
    if (current != 0) current->previous = node;
    node->key = key;
    node->voice = state->id;
    state->head.node = node;
    return key;
}

/* The id of the first voice a handle names, or -1 if the handle is no longer valid. */
int sound_resolve_handle(u32 key)
{
    HandleNode *node;
    if (key != VOICE_NONE) {
        node = sound_find_handle(key);
        if (node != 0) return node->voice;
    }
    return -1;
}

/* Empties the priority lists. */
void sound_init_priority_lists(void)
{
    u32 i;
    for (i = 0; i < sound_voice_count; i++) {
        sound_voice_priority_links[i].active = 0;
    }
    for (i = 0; i < 256; i++) {
        sound_priority_first_voice[i] = LINK_END;
    }
    sound_lowest_priority = LEVEL_END;
}

/* Takes a voice out of its priority's list, and the priority out of the list of priorities in use if it was the last
 * voice there. */
void sound_remove_voice_priority(SynthVoice *state)
{
    VoicePriorityLink *link;
    PriorityLevelLink *group;
    link = &sound_voice_priority_links[state->id & VOICE_INDEX_MASK];
    if (link->active == 1) {
        if (link->previous != LINK_END) sound_voice_priority_links[link->previous].next = link->next;
        else sound_priority_first_voice[state->priority] = link->next;
        if (link->next != LINK_END) sound_voice_priority_links[link->next].previous = link->previous;
        else if (link->previous == LINK_END) {
            group = &sound_priority_levels[state->priority];
            if (group->previous != LEVEL_END) sound_priority_levels[group->previous].next = group->next;
            else sound_lowest_priority = group->next;
            if (group->next != LEVEL_END) sound_priority_levels[group->next].previous = group->previous;
        }
        link->active = 0;
    }
}

/* Puts a voice at the head of the list of priority (moving it from another list), inserting the priority in the sorted
 * list of priorities in use when it is new there. */
void sound_set_voice_priority(SynthVoice *state, u8 priority)
{
    u16 level;
    u16 previous;
    VoicePriorityLink *link;
    u8 index;
    index = state->id & VOICE_INDEX_MASK;
    link = &sound_voice_priority_links[index];
    if (link->active == 1) {
        if (state->priority == priority) return;
        sound_remove_voice_priority(state);
    }
    link->active = 1;
    link->previous = LINK_END;
    if ((link->next = sound_priority_first_voice[priority]) != LINK_END) {
        sound_voice_priority_links[sound_priority_first_voice[priority]].previous = index;
    } else {
        if (sound_lowest_priority != LEVEL_END) {
            if (priority >= sound_lowest_priority) {
                for (level = sound_lowest_priority; level != LEVEL_END; level = sound_priority_levels[level].next) {
                    if (priority < level) break;
                    previous = level;
                }
                sound_priority_levels[previous].next = priority;
                sound_priority_levels[priority].previous = previous;
                sound_priority_levels[priority].next = level;
                if (sound_priority_levels[priority].next != LEVEL_END) sound_priority_levels[level].previous = priority;
            } else {
                sound_priority_levels[priority].next = sound_lowest_priority;
                sound_priority_levels[priority].previous = LEVEL_END;
                sound_priority_levels[sound_lowest_priority].previous = priority;
                sound_lowest_priority = priority;
            }
        } else {
            sound_priority_levels[priority].next = LEVEL_END;
            sound_priority_levels[priority].previous = LEVEL_END;
            sound_lowest_priority = priority;
        }
    }
    sound_priority_first_voice[priority] = index;
    state->priority = priority;
}

/* Finds a voice for a new sound of this priority, at most maxVoices of whose voices may play at once (sounds of one
 * allocId count together); fxFlag says whether it is an effect or music. Returns the voice's index, or -1 (also while
 * sound_is_idle holds the voices: sound_voices_locked).
 *
 * at_quota: the sound's class (effects or music) uses at least its quota (sound_sfx_voice_limit,
 * sound_music_voice_limit) of voices, and the quota is below the voice count; it may then only take a voice of its own
 * class. Unless the quota is no more than maxVoices, the voices of this allocId are counted, at priorities up to this
 * one first, keeping the oldest eligible as the voice to steal: at maxVoices or more the sound takes that voice (its
 * own oldest). Below, it takes a free voice if there is one and it is not at its quota, else steals the oldest
 * eligible voice whose priority is no higher; either way it fails if that voice's priority is higher than its own.
 * A voice taken from the free list leaves it; a stolen one is counted out of its class. A voice allocated this tick
 * (VOICE_STARTING) and a stream's voice are never stolen. On the PC each refusal and theft is counted (--log-voices). */
int sound_alloc_voice(u8 priority, u8 maxVoices, u16 allocId, u8 fxFlag)
{
    s32 i;
    s32 count;
    s32 voice;
    u16 level;
    u32 at_quota;
    ChannelLink *free_link;

    if (!sound_voices_locked) {
        if (fxFlag) {
            at_quota = (sound_sfx_voices_used >= (i = sound_sfx_voice_limit) && sound_voice_count > i) ? 1 : 0;
            if (i <= maxVoices) {
                goto take_free_or_steal;
            }
            goto count_own_voices;
        } else {
            at_quota = (sound_music_voices_used >= (i = sound_music_voice_limit) && sound_voice_count > i) ? 1 : 0;
            if (i <= maxVoices) {
                goto take_free_or_steal;
            }
        count_own_voices:
            count = 0;
            voice = -1;
            level = sound_lowest_priority;
            while (level != LEVEL_END && priority >= level && voice == -1) {
                for (i = sound_priority_first_voice[level]; i != LINK_END; i = sound_voice_priority_links[i].next) {
                    if (VOICE_AT(i).blocked == 0 && allocId == VOICE_AT(i).allocId) {
                        ++count;
                        if ((!at_quota || fxFlag == sound_synth_voices[i].fxFlag) && !(sound_synth_voices[i].flags & VOICE_STARTING)) {
                            if (voice != -1) {
                                if (sound_synth_voices[i].age < sound_synth_voices[voice].age)
                                    voice = i;
                            } else
                                voice = i;
                        }
                    }
                }
                level = sound_priority_levels[level].next;
            }
        }
        if (count < maxVoices) {
            do {} while (0);
            while (level != LEVEL_END && count < maxVoices) {
                i = sound_priority_first_voice[level];
                while (i != LINK_END) {
                    if (!VOICE_AT(i).blocked) {
                        if (allocId == VOICE_AT(i).allocId) {
                            count++;
                        }
                    }
                    i = sound_voice_priority_links[i].next;
                }
                level = sound_priority_levels[level].next;
            }
            if (count < maxVoices) {
            take_free_or_steal:
                voice = -1;
                if (sound_voice_free_head != LINK_END && at_quota == 0) {
                    voice = sound_voice_free_head;
                } else {
                if (priority < sound_lowest_priority) {
                    pc_snd_count(PC_SND_MX_REFUSED); return -1;
                }
                level = sound_lowest_priority;
                while (level != LEVEL_END && priority >= level && voice == -1) {
                    for (i = sound_priority_first_voice[level]; i != LINK_END; i = sound_voice_priority_links[i].next) {
                        if (VOICE_AT(i).blocked == 0 && (!at_quota || fxFlag == sound_synth_voices[i].fxFlag) && !(sound_synth_voices[i].flags & VOICE_STARTING)) {
                            if (voice != -1) {
                                if (sound_synth_voices[voice].age > sound_synth_voices[i].age)
                                    voice = i;
                            } else
                                voice = i;
                        }
                    }
                    level = sound_priority_levels[level].next;
                }
                if (voice == -1) {
                    pc_snd_count(PC_SND_MX_REFUSED); return -1;
                }
                }
                if (sound_synth_voices[voice].priority > priority) {
                    goto refuse;
                }
            }
        }
        if (voice == -1) {
            goto refuse;
        }
        if (sound_voice_free_links[voice].flags == 1) {
            free_link = &sound_voice_free_links[voice];
            i = free_link->previous;
            if (i != LINK_END) {
                sound_voice_free_links[i].next = free_link->next;
            } else {
                sound_voice_free_head = free_link->next;
            }
            i = free_link->next;
            if (i != LINK_END) {
                sound_voice_free_links[i].previous = free_link->previous;
            }
            if (voice == sound_voice_free_tail) {
                sound_voice_free_tail = free_link->previous;
            }
            free_link->flags = 0;
        } else if (sound_synth_voices[voice].fxFlag) {
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
refuse:
    pc_snd_count(PC_SND_MX_REFUSED); return -1;
}

/* Returns a voice to the free list (once): out of its priority list, macro and priority cleared, counted out of its
 * class, its id VOICE_NONE. */
void sound_free_voice(SynthVoice *state)
{
    u32 index;
    ChannelLink *link;
    sound_remove_voice_priority(state);
    index = state->id & VOICE_INDEX_MASK;
    link = &sound_voice_free_links[index];
    state->head.addr = 0;
    state->priority = 0;
    if (link->flags == 0) {
        link->flags = 1;
        if (sound_voice_free_head != LINK_END) {
            link->previous = sound_voice_free_tail;
            link->next = LINK_END;
            sound_voice_free_links[sound_voice_free_tail].next = index;
        } else {
            link->next = LINK_END;
            link->previous = LINK_END;
            sound_voice_free_head = index;
        }
        sound_voice_free_tail = index;
        if (state->fxFlag) {
            --sound_sfx_voices_used;
        } else {
            --sound_music_voices_used;
        }
    }
    state->id = VOICE_NONE;
}

/* Links every voice into the free list, in order (the library's start-up). */
void sound_init_free_voices(void)
{
    u32 i;
    for (i = 0; i < sound_voice_count; i++) {
        sound_voice_free_links[i].previous = i - 1;
        sound_voice_free_links[i].next = i + 1;
        sound_voice_free_links[i].flags = 1;
    }
    sound_voice_free_links[0].previous = LINK_END;
    sound_voice_free_links[sound_voice_count - 1].next = LINK_END;
    sound_voice_free_head = 0;
    sound_voice_free_tail = sound_voice_count - 1;
}

/* Frees every voice and empties the priority lists and the class counts. */
void sound_init_voice_allocation(void)
{
    sound_init_free_voices();
    sound_init_priority_lists();
    sound_sfx_voices_used = 0;
    sound_music_voices_used = 0;
}

/* Takes a voice for a stream (streams_3d.c) at this priority: blocked from the synth, an effect's, out of any chain,
 * its id (VOICE_NONE with the index in the low byte), its hardware voice stopped. Returns the index, or -1. */
int sound_alloc_stream_voice(u8 priority)
{
    int index;
    SynthVoice *state;
    index = sound_alloc_voice(priority, 255, ALLOC_ID_NONE, 1);
    if (index != -1) {
        state = &sound_synth_voices[index];
        state->blocked = 1;
        state->fxFlag = 1;
        sound_unlink_voice(state);
        state->id = (u32)index | 0xFFFFFF00U;
        if (sound_hw_is_voice_active(index)) sound_hw_break_voice(index);
        state->head.addr = 0;
        sound_set_voice_priority(state, priority);
    }
    return index;
}

/* Gives a stream's voice back: stops the hardware voice and frees it. */
void sound_free_stream_voice(u32 index)
{
    if (index == VOICE_NONE) {
        return;
    }
    if (sound_hw_is_voice_active(index)) {
        sound_hw_break_voice(index);
    }
    sound_synth_voices[index].id = index;
    sound_free_voice(&sound_synth_voices[index]);
    sound_synth_voices[index].blocked = 0;
}

/* Ends a voice: out of its chain, its start flags (bits 0 and 1: VOICE_STARTING) and age cleared, freed. */
void sound_end_voice(SynthVoice *state)
{
    sound_unlink_voice(state);
    state->flags &= ~3U;
    state->age = 0;
    sound_free_voice(state);
}

/* Stops every voice of a handle's chain at once: ends the voices whose macro runs, and keys off the hardware voices.
 * The caller holds the library lock. Returns 0, or -1 if the library is not started or the handle is not valid. */
int sound_stop_unlocked(u32 key)
{
    int result;
    u32 index;
    u32 next;
    result = -1;
    if (sound_initialized) {
        key = sound_resolve_handle(key);
        while (key != VOICE_NONE) {
            index = key & VOICE_INDEX_MASK;
            next = sound_synth_voices[index].head.next;
            if (key == sound_synth_voices[index].id) {
                result = 0;
                if (sound_synth_voices[index].head.addr != 0) {
                    sound_end_voice(&sound_synth_voices[index]);
                }
                sound_hw_key_off(index);
            }
            key = next;
        }
    }
    return result;
}

/* Ends every voice whose macro runs, and keys off every hardware voice; with preserve_external, leaves the effects'
 * voices playing. */
void sound_stop_all_voices(u8 preserve_external)
{
    int i;
    for (i = 0; i < sound_voice_count; i++) {
        if (sound_synth_voices[i].head.addr != 0) {
            if (!preserve_external ||
                (preserve_external && !sound_synth_voices[i].fxFlag)) {
                sound_end_voice(&sound_synth_voices[i]);
                sound_synth_voices[i].head.addr = 0;
                sound_synth_voices[i].flags &= ~3U;
            } else {
                continue;
            }
        }
        sound_hw_key_off(i);
    }
}
