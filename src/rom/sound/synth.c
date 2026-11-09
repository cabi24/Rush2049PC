/* src/rom/sound/synth.c: the MusyX sound library's synth: starting notes and the per-tick voice update.
 *
 * A sound starts here: an effect by its id (sound_start_sfx_unlocked, from its effect table), a note by its macro,
 * keymap or layer (sound_synth_start_note, sound_synth_start_layer, from the sequencer and the macros), each voice
 * then running a SoundMacro (macros.c) on a voice voices.c allocates; a held note on a channel with portamento on is
 * retriggered legato instead. Every tick (sound_synth_tick, from the hardware layer's frame service) the voices' macros
 * run and sound_synth_update_voices turns each voice's state (note, detune, pitch bend, vibrato, sweeps, portamento;
 * volume, pan, envelopes, the volume group faders, the MIDI controllers) into the hardware layer's pitch and volumes;
 * the volume group faders (sound_volume_faders) advance their fades. The sfx setters (sound_set_sfx_*_unlocked) write a
 * sound's MIDI controllers along its voice chain.
 *
 * MusyX is the N64's sound library: no MAT3 counterpart. The voices are seen through this file's partial views, at the
 * N64 offsets (each field's name ends in its offset); on the PC their storage is PC data, PC_MX_VOICE_SLOTS long
 * (pc_game_types.h, docs/port/DECOUPLE.md 9). Pitches are 16.16 note numbers until sound_synth_key_to_pitch turns
 * them into the hardware's 4.12 playback rate; volumes and pans are 16.16, 0x7F0000 the loudest or rightmost. */
#include "../rom_tu.h"
#include "musyx.h"

#define TICK_RATE_SFX 8            /* sound_tick_rates: [0..7] the song players', [8] the effects' */
#define VOICE_MIDI_SET_BYTE 0x4B   /* a voice's midiSet4B, as sound_synth_get_tick_rate reads it */

#define CONTROLLER_SWITCH_ON 0x1F80        /* a 14-bit switch controller (pedal, portamento) above this is on */
#define PITCH_BEND_CENTER 0x2000

/* What a finished fade then does to its song (seqMode). */
#define FADE_THEN_STOP 1
#define FADE_THEN_PAUSE 2
#define FADE_THEN_MUTE 3

/* sound_synth_apply_portamento's view of a voice. */
#pragma pack(1)
typedef struct RampState { u8 unknown00[36]; u32 flags24; u8 unknown28[100]; u32 portTime8C; u32 portDuration90; u32 portCurrent94; } RampState;
#pragma pack(0)

/* sound_synth_start_portamento's (and sound_synth_copy_controllers') view of a voice. */
#pragma pack(1)
typedef struct VoiceState {
    u8 unknown00[36];
    u32 flags24;
    u8 unknown28[34];
    u8 midi4A;             /* the MIDI channel, or NO_CHANNEL */
    u8 midiSet4B;          /* the channel's set: a song player, or 255 for an effect's own controllers */
    u8 unknown4C[64];
    u32 portTime8C;        /* portamento: time gone, of portDuration90 */
    u32 portDuration90;
    u32 portCurrent94;     /* portamento: the pitch it has reached, 16.16 */
    u8 portType98;         /* 1: glide only on a voice's first update */
    u8 unknown99[40];
    u8 lastNoteC1;         /* the note the glide starts from */
} VoiceState;
#pragma pack(0)

/* The voices as most of this file reads them. */
#pragma pack(1)
typedef struct SynthVoiceLegatoView {
    u32 addr00;            /* the macro's first step; 0: no macro runs */
    u8 unknown04[12];
    u32 child10, parent14; /* the voice chain: next and previous voice ids */
    u8 unknown18[12];
    u32 flags24;
    u32 age28;
    u8 unknown2C[2];
    u8 priority2E, unknown2F;
    u32 volume30;
    u8 unknown34[4];
    u32 panning38;
    u8 unknown3C[4];
    u32 sweepOff40[2];
    u8 sweepNum48[2];
    u8 midi4A, midiSet4B;
    u8 unknown4C[2];
    u16 orgNote4E, note50; /* the note started, the note now */
    u8 unknown52[3];
    u8 setupMidiSet55;     /* the controllers' set until the macro's first step copies it to midiSet4B */
    u8 unknown56[10];
    u32 identifier60;      /* the voice's id; VOICE_NONE when free */
    u8 unknown64[4];
    u16 field68;
    u8 unknown6A[34];
    u32 portTime8C;
    u8 unknown90[4];
    u32 portCurrent94;
    u8 portType98, revVolScale99, revVolOffset9A;
    u8 unknown9B[34];
    u8 blockedBD, vGroupBE, unknownBF;
    s8 detuneC0;           /* cents */
    u8 lastNoteC1;
    u16 curOutputVolumeC2;
    u8 unknownC4[168];
    u32 lfoPeriod16C;
    u16 lfoValue170;
    u8 unknown172[6];
    u32 lfoPeriod178;
    u16 lfoValue17C;
    u8 unknown17E[34];
} SynthVoiceLegatoView;
#pragma pack(0)
/* On the PC the voices are PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9), seen here in this file's view; PC_MX_LOOP(32) is the run's voices where the N64 sets up
 * its 32 (docs/port/AUDIO.md 10). */
#define sound_synth_voices (*(SynthVoiceLegatoView (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)

/* A layer of a layer list (registry.c): a key range plays the macro id with its own transpose, volume, priority and
 * pan (bit 7 of panning: keep the note's pan as given). */
#pragma pack(1)
typedef struct LayerEntry { u16 id; u8 low; u8 high; s8 transpose; u8 volume; s16 priority; u8 panning; u8 unknown09[3]; } LayerEntry;
#pragma pack(0)

/* A keymap entry, one per key: the macro (or, with bit 15, the layer list) the key plays, transposed, panned and
 * prioritized; id 0xFFFF: the key plays nothing. */
#pragma pack(1)
typedef struct Keymap { u16 id; s8 transpose; u8 panning; s16 priority; u8 unknown06[2]; } Keymap;
#pragma pack(0)

/* sound_synth_key_to_pitch's view: the sample's key and rate (key << 24 | rate in Hz), set by the macro's sample
 * start. */
#pragma pack(1)
typedef struct VoicePrefix { u8 unknown00[92]; u32 sampleInfo5C; } VoicePrefix;
#pragma pack(0)

/* A volume group fader, the view sound_is_group_fading_out and sound_synth_init read. */
typedef struct ChannelState {u32 volume,target,delta,time,seqId;u8 type;u8 unknown15[3];u32 pauseVolume;u8 unknown1C[12];} ChannelState;
extern ChannelState sound_volume_faders[32];

/* sound_synth_update_voices's view of a voice. */
#pragma pack(1)
typedef struct SynthLfo { u32 time; u32 period; s16 value; s16 lastValue; } SynthLfo;
/* PC: +0 is the voice's macro start, a 32-bit window address (macros.c) */
typedef struct SynthVoice { u32 addr00; u8 unknown04[0x20]; u32 cFlags24; u32 age28; u16 ageSpeed2C; u8 unknown2E; u8 track2F; s32 volume30; u8 unknown34[4]; s32 panning38; s32 panDelta3C; s32 sweepOff40[2]; u8 sweepNum48[2]; u8 midi4A; u8 midiSet4B; u8 fxFlag4C; u8 unknown4D[3]; u16 curNote50; u8 unknown52[0x18]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u32 vibCurTime6C; u32 vibPeriod70; s32 vibCurOffset74; u8 vibKeyRange78; u8 vibCentRange79; u8 unknown7A[2]; s32 sweepAdd7C[2]; s32 envDelta84; s32 envTarget88; u8 unknown8C[0xD]; u8 revVolScale99; u8 revVolOffset9A; u8 unknown9B[0xD]; u32 panTimeA8; s32 panTargetAC; u8 sweepCntB0[2]; u8 unknownB2[2]; u32 envTimeB4; u16 pbLastB8; u8 unknownBA[3]; u8 blockedBD; u8 vGroupBE; u8 unknownBF; s8 curDetuneC0; u8 unknownC1; u16 curOutputVolumeC2; u8 unknownC4[0xA4]; SynthLfo lfo168[2]; s16 localVars180[16]; } SynthVoice;
#pragma pack(0)
/* A volume group fader as the update reads it: the volume and its fade, the song a fade then stops, pauses or mutes,
 * and the pause volume with its own fade. */
typedef struct SynthFader { s32 volume; s32 target; s32 delta; s32 time; u32 seqId; u8 type; u8 seqMode; u8 unknown16[2]; s32 pauseVolume; s32 pauseTarget; s32 pauseDelta; s32 pauseTime; } SynthFader;
#define VOLUME_FADERS ((SynthFader *)sound_volume_faders)

/* An effect of an effect table (registry.c's Descriptor). */
#pragma pack(1)
typedef struct Preset {
    u16 identifier;
    u16 macro_id;          /* the macro, keymap or layer it plays */
    u8 max_voices;
    u8 priority;
    u8 volume;
    u8 pan;
    u8 key;
    u8 volume_group;
    u8 unknown0A[2];
} Preset;
#pragma pack(0)

/* A volume group fader, the view the fades set. */
typedef struct MasterFader { u32 volume; u32 target; s32 delta; u32 time; u32 seqId; u8 type; u8 seqMode; u8 unknown16[2]; u32 pauseVolume; u32 pauseTarget; s32 pauseDelta; u32 pauseTime; } MasterFader;

extern unsigned int sound_tick_rates[];
extern u32 sound_tick_step;
extern void sound_midi_set_portamento_state(u8, u8, u8);
extern u8 sound_voice_count;
extern u8 sound_hw_is_voice_active(u32);
extern void sound_unlink_voice(SynthVoiceLegatoView *);
extern u32 sound_new_handle(SynthVoiceLegatoView *);
extern void sound_synth_claim_channel(SynthVoiceLegatoView *);
extern void sound_midi_set_last_key(u8,u8,u8);
extern u8 sound_midi_get_portamento_state(u8, u8);
extern void *sound_find_layer(u16, u16 *);
extern u32 sound_macro_start(u32, u16, u8, u8, u8, u8, u8, u16, u16, u8, u8);
extern Keymap *sound_find_keymap(u16);
extern u32 sound_note_to_pitch(u8, u32);
extern u16 sound_pitch_up_semitone(u16);
extern u8 sound_track_volumes[];
extern u8 sound_mono;
extern void sound_macro_run(void *);
extern short sound_sine(u16);
extern unsigned short sound_synth_get_volume_input(void *);
extern unsigned short sound_synth_get_pan_input(void *);
extern unsigned short sound_synth_get_span_input(void *);
extern unsigned short sound_synth_get_pitch_bend_input(void *);
extern unsigned short sound_synth_get_doppler_input(void *);
extern unsigned short sound_synth_get_modulation_input(void *);
extern unsigned short sound_synth_get_pedal_input(void *);
extern unsigned short sound_synth_get_portamento_input(void *);
extern unsigned short sound_synth_get_reverb_input(void *);
extern void sound_hw_start_voice(unsigned int);
extern void sound_hw_release_hold(int);
extern void sound_hw_set_pitch(int, unsigned short);
extern void sound_hw_set_volume(int, unsigned int, unsigned int, unsigned int, unsigned int);
extern void sound_hw_commit_voices(void);
extern void sound_seq_pause_unlocked(unsigned int);
extern void sound_seq_stop_unlocked(unsigned int);
extern void sound_seq_set_track_mask_unlocked(u32, u32, u32);
/* PC: a voice the driver still plays whose macro has ended, counted when its sample loops (--log-voices: such a
 * voice plays until it is taken again; hardware.c's pc_mx_note_macroless). */
extern void pc_mx_note_macroless(int index);
extern s32 sound_samples_per_tick;
extern s32 sound_synth_frequency;
extern Preset *sound_find_sfx(u16);
extern int sound_resolve_handle(u32);
extern void sound_midi_set_controller(u8, u8, u8, u8);
extern void sound_midi_copy_voice_controller(u8, struct VoiceState *, struct VoiceState *);
extern u8 sound_initialized;
extern void sound_ms_to_time(u32 *);
extern unsigned char sound_volume_fader_types[][40];
extern short sound_global_variables[16];
extern u32 sound_synth_unread_setting;
extern void sound_init_handles(void);
extern void sound_init_voice_allocation(void);
extern void sound_synth_init_channel_owners(void);

/* Sets the tick rate of song player index (255: the effects') from a tempo in beats per minute. */
void sound_synth_set_tempo(unsigned int tempo, unsigned char player)
{
    if (player == 255) {
        player = TICK_RATE_SFX;
    }
    sound_tick_rates[player] = (tempo << 3) * 1536 / 240;
}

/* The tick rate of the player whose MIDI set the voice has (an effect's voice: the effects'). */
unsigned int sound_synth_get_tick_rate(unsigned char *state)
{
    return sound_tick_rates[state[VOICE_MIDI_SET_BYTE] == 255 ? TICK_RATE_SFX : state[VOICE_MIDI_SET_BYTE]];
}

/* Glides the voice's pitch toward target (a 16.16 note) while portamento is on: one tick's share of what is left of the
 * glide time. Returns the pitch to play: the glide's, or target once it is reached or the glide is over. */
u32 sound_synth_apply_portamento(RampState *state, u32 target)
{
    s32 remaining;
    u32 previous;
    u32 elapsed;
    u32 total;
    if (state->flags24 & VOICE_PORTAMENTO) {
        total = state->portDuration90;
        elapsed = state->portTime8C;
        remaining = (total - elapsed) >> 8;
        if (remaining > 0) {
            previous = state->portCurrent94;
            state->portCurrent94 += (s32)(sound_tick_step * (u32)((s32)(target - state->portCurrent94) >> 8)) / remaining;
            if ((previous < target && state->portCurrent94 < target) ||
                (previous > target && state->portCurrent94 > target)) {
                target = state->portCurrent94;
                state->portTime8C = elapsed + sound_tick_step;
            } else {
                state->portTime8C = total;
            }
        }
    }
    return target;
}

/* Starts a glide (unless the voice was retriggered legato): from the channel's last note (lastNoteC1), over the
 * portamento time, or straight to the note (time already gone) for type 1 past the voice's first update; marks
 * portamento on for the voice's MIDI channel. */
void sound_synth_start_portamento(VoiceState *state)
{
    if (!(state->flags24 & VOICE_LEGATO)) {
        if (state->portType98 == 1) {
            if (!(state->flags24 & VOICE_UPDATED)) state->portTime8C = 0;
            else state->portTime8C = state->portDuration90;
        } else {
            state->portTime8C = state->portDuration90;
        }
        state->portCurrent94 = (u32)state->lastNoteC1 << 16;
    }
    if (state->midi4A != NO_CHANNEL) sound_midi_set_portamento_state(state->midi4A, state->midiSet4B, 1);
}

/* Legato: instead of starting new voices, gives the playing, held voices of a MIDI channel (channel, set) the new key
 * (keeping each voice's transpose from its original note), gliding from where each was, and makes them one new chain
 * with a new handle. Returns the handle, or VOICE_NONE if the channel had no such voice. */
u32 sound_synth_retrigger_legato(u8 key,u8 channel,u8 set)
{
    u32 i,result,previous;
    SynthVoiceLegatoView *state,*last;
    result=VOICE_NONE;
    for (i=0,state=sound_synth_voices;i<sound_voice_count;i++,state++) {
        if (state->identifier60!=VOICE_NONE && state->midi4A==channel && state->midiSet4B==set &&
            (state->flags24&VOICE_PLAYING) && (!(state->flags24&VOICE_KEY_OFF) || (state->flags24&VOICE_PEDAL_DOWN)) &&
            sound_hw_is_voice_active(i)) {
            last=state;
            state->portCurrent94=((u32)state->note50<<16)+(state->detuneC0*65536)/100;
            state->lastNoteC1=state->note50;
            state->note50=key+(state->note50&255)-(u8)state->orgNote4E;
            state->orgNote4E=key;
            state->detuneC0=0;
            state->portTime8C=0;
            state->flags24|=VOICE_LEGATO|VOICE_PORTAMENTO;
            sound_unlink_voice(&sound_synth_voices[i]);
            if (result==VOICE_NONE) {
                state->child10=VOICE_NONE;
                state->parent14=VOICE_NONE;
                result=sound_new_handle(&sound_synth_voices[i]);
                previous=state->identifier60;
            } else {
                sound_synth_voices[(previous&VOICE_INDEX_MASK)].child10=state->identifier60;
                state->parent14=previous;
                previous=state->identifier60;
            }
        }
    }
    if (result!=VOICE_NONE) {
        sound_synth_claim_channel(last);
        sound_midi_set_last_key(last->midi4A,last->midiSet4B,(u8)last->note50);
    }
    return result;
}

/* Retriggers legato (sound_synth_retrigger_legato) when the MIDI channel has portamento on. Returns the handle, or
 * VOICE_NONE to start the note normally. */
u32 sound_synth_try_legato(u8 key, u8 channel, u8 set)
{
    u32 handle;
    if (channel != NO_CHANNEL && sound_midi_get_portamento_state(channel, set) != 0) {
        handle = sound_synth_retrigger_legato(key & 127, channel, set);
        if (handle != VOICE_NONE) {
            return handle;
        }
    }
    return VOICE_NONE;
}

/* Starts a layer list's note: a macro for every layer whose key range has the key, each with the layer's transpose,
 * pan, volume and priority, chained into one sound. packed is (layer id << 16 | priority << 8 | voice limit); a
 * legato retrigger takes the place of the whole note. Returns the handle of the chain, or VOICE_NONE. */
u32 sound_synth_start_layer(u32 packed, u16 allocation, u8 key, u8 volume,
                 u8 panning, u8 channel, u8 set, u16 offset, u16 section, u8 group)
{
    u16 count;
    u32 result;
    u32 previous;
    u32 current;
    u32 nextPacked;
    int i;
    int note;
    int pan;
    int priority;
    LayerEntry *layer;
    LayerEntry *layers;
    u8 layer_volume;
    result = VOICE_NONE;
    layers = sound_find_layer(packed >> 16, &count);
    if (layers != 0) {
        for (i = 0; i < count; i++) {
            layer = &layers[i];
            if (layer->id != 0xFFFF && layer->low <= (key & 127) && layer->high >= (key & 127)) {
            note = layer->transpose + (key & 127);
            note = note > 127 ? 127 : note < 0 ? 0 : note;
            current = sound_synth_try_legato(note, channel, set);
            if (current != VOICE_NONE) return current;
            if ((layer->panning & 128) == 0) {
                pan = layer->panning - 64;
                pan += panning;
                pan = pan < 0 ? 0 : pan > 127 ? 127 : pan;
            } else pan = 128;
            layer_volume = (layer->volume * volume) / 127;
            priority = ((packed >> 8) & 255) + layer->priority;
            priority = priority > 255 ? 255 : priority < 0 ? 0 : priority;
            nextPacked = (packed & 255) | ((u32)priority << 8) | ((u32)layer->id << 16);
            switch (layer->id & 0xC000) {
            case 0:
                if (result == VOICE_NONE) {
                    previous = sound_macro_start(nextPacked, allocation, note | (key & 128), layer_volume,
                        pan, channel, set, offset, section, 0, group);
                    if (previous != VOICE_NONE) result = sound_new_handle(&sound_synth_voices[previous & VOICE_INDEX_MASK]);
                } else {
                    current = sound_macro_start(nextPacked, allocation, note | (key & 128), layer_volume,
                        pan, channel, set, offset, section, 0, group);
                    if (current != VOICE_NONE) {
                        sound_synth_voices[previous & VOICE_INDEX_MASK].child10 = current;
                        sound_synth_voices[current & VOICE_INDEX_MASK].parent14 = previous;
                        previous = current;
                    }
                }
                break;
            }
            }
        }
    }
    return result;
}

/* Starts a note of a macro, keymap or layer list: packed is (id << 16 | priority << 8 | voice limit); the id's top bits
 * say which (0 a macro, 0x4000 a keymap, 0x8000 a layer list); key has bit 7 set for an effect. A keymap maps the key
 * to a macro or layer list with its transpose, pan and priority. channel and set are the MIDI channel (255 none),
 * offset the macro step to start at, section and group the voice's song section and volume group; priorityOffset is
 * added to the priority. A legato retrigger can take the place of the note. Returns the handle, or VOICE_NONE. */
u32 sound_synth_start_note(u32 packed, u8 key, u8 volume, u8 panning,
                 u8 channel, u8 set, u16 offset, u16 section, u8 group, s16 priorityOffset)
{
    Keymap *keymap;
    u16 id;
    int mapped_id;
    int pan_sum;
    int priority;
    int note;
    id = packed >> 16;
    priority = (int)((packed >> 8) & 255) + priorityOffset;
    priority = priority < 0 ? 0 : priority > 255 ? 255 : priority;
    packed = (packed & 0xFFFF00FFU) | ((u32)priority << 8);
    switch (id & 0xC000) {
    case 0:
        /* priority holds the legato handle from here (the original reuses the variable) */
        priority = sound_synth_try_legato(key, channel, set);
        if (priority != VOICE_NONE) return priority;
        return sound_macro_start(packed, packed >> 16, key, volume, panning, channel, set, offset, section, 1, group);
    case 0x4000:
        keymap = sound_find_keymap(id);
        if (keymap != 0) {
            if ((mapped_id = keymap[(u8)(key & 127)].id) != 0xFFFF) {
                if ((keymap[(u8)(key & 127)].panning & 128) == 0) {
                    /* Original game: keymap[key], not keymap[key & 127] as the lines around it: a key with
                     * bit 7 set (an effect's) reads an entry past the 128 of the keymap. */
                    pan_sum = keymap[key].panning - 64;
                    pan_sum += panning;
                    if (pan_sum < 0) panning = 0;
                    else if (pan_sum > 127) panning = 127;
                    else panning = pan_sum;
                } else panning = 128;
                note = (key & 127) + keymap[(u8)(key & 127)].transpose;
                note = note > 127 ? 127 : note < 0 ? 0 : note;
                priority += keymap[(u8)(key & 127)].priority;
                priority = priority > 255 ? 255 : priority < 0 ? 0 : priority;
                packed &= 255;
                packed |= (u32)priority << 8;
                packed |= (u32)mapped_id << 16;
                if ((mapped_id & 0xC000) == 0) {
                    priority = sound_synth_try_legato(note, channel, set);
                    if (priority != VOICE_NONE) return priority;
                    return sound_macro_start(packed, id, note | (key & 128), volume, panning, channel, set, offset, section, 1, group);
                }
                return sound_synth_start_layer(packed, id, note | (key & 128), volume, panning, channel, set, offset, section, group);
            }
        }
        break;
    case 0x8000:
        return sound_synth_start_layer(packed, packed >> 16, key, volume, panning, channel, set, offset, section, group);
    }
    return VOICE_NONE;
}

/* The hardware playback rate (4.12) for a 16.16 note on the voice's sample: the whole note's rate, plus the fraction
 * of the way to the next semitone. */
u32 sound_synth_key_to_pitch(VoicePrefix *state, u32 note)
{
    u32 fraction;
    u32 pitch;
    pitch = sound_note_to_pitch(note >> 16, state->sampleInfo5C) << 16;
    fraction = note & 65535;
    if (fraction != 0) {
        pitch += (sound_pitch_up_semitone(pitch >> 16) - (pitch >> 16)) * fraction;
    }
    return pitch;
}

/* The synth's tick: for every voice, runs its macro (sound_macro_run), then (unless it is a stream's) advances its
 * LFOs, vibrato, pitch sweeps, pedal and portamento state, pan and volume sweeps, starts or releases its sample, and
 * sets the hardware voice's pitch (note, detune, pitch bend, vibrato, sweeps, portamento, doppler) and volumes (the
 * voice's volume through its group's pause and fade volumes, the class's master fader, its song track's volume and the
 * MIDI volume; pan, surround span and reverb send from the controllers, or centred in mono). Then advances every
 * fader's fade, stopping, pausing or muting a song whose fade ended, and commits the voices to the hardware layer. */
void sound_synth_update_voices(void)
{
    SynthVoice *sv;
    s32 reverb;
    s32 i;
    s32 j;
    u32 note_pitch;
    s32 pitch_bend;
    s32 vibrato;
    s32 span;
    u32 pitch;
    s32 vol;
    s32 pan;
    s32 vibrato_range;
    u8 midi;

    for (i = 0, sv = (SynthVoice *)sound_synth_voices; i < sound_voice_count; i++, sv++) {
        if (sv->addr00 != 0) {
            sound_macro_run(sv);
        } else if (!sound_hw_is_voice_active(i)) {
            continue;
        }
        if (sv->addr00 == 0) pc_mx_note_macroless(i);   /* PC: --log-voices counts the voices playing without a macro */
        if (sv->blockedBD != 0) {
            continue;
        }
        for (j = 0; j < 2; j++) {
            if (sv->lfo168[j].period != 0) {
                sv->lfo168[j].time += sound_tick_step;
                sv->lfo168[j].value = sound_sine(((sv->lfo168[j].time % sv->lfo168[j].period) * 16) / (sv->lfo168[j].period >> 8));
            }
        }
        if (sv->cFlags24 & VOICE_VIBRATO) {
            sv->vibCurTime6C += sound_tick_step;
            sv->vibCurOffset74 = sound_sine(((sv->vibCurTime6C % sv->vibPeriod70) * 16) / (sv->vibPeriod70 >> 8));
        }
        for (j = 0; j < 2; j++) {
            if (sv->sweepNum48[j] != 0) {
                if (--sv->sweepCntB0[j] == 0) {
                    sv->sweepCntB0[j] = sv->sweepNum48[j];
                    sv->sweepOff40[j] = 0;
                } else {
                    sv->sweepOff40[j] += sv->sweepAdd7C[j];
                }
            }
        }
        if (sv->midi4A != NO_CHANNEL) {
            if (sound_synth_get_pedal_input(sv) <= CONTROLLER_SWITCH_ON) {
                sv->cFlags24 &= ~VOICE_PEDAL_DOWN;
            } else {
                sv->cFlags24 |= VOICE_PEDAL_DOWN;
            }
        } else {
            sv->cFlags24 &= ~VOICE_PEDAL_DOWN;
        }
        if (!(sv->cFlags24 & VOICE_PORTAMENTO_BY_MACRO)) {
            if (sv->midi4A != NO_CHANNEL) {
                if (sound_synth_get_portamento_input(sv) <= CONTROLLER_SWITCH_ON) {
                    sv->cFlags24 &= ~VOICE_PORTAMENTO;
                    sound_midi_set_portamento_state(sv->midi4A, sv->midiSet4B, 0);
                } else {
                    if (!(sv->cFlags24 & VOICE_PORTAMENTO)) {
                        sound_synth_start_portamento((struct VoiceState *)sv);
                    }
                    sv->cFlags24 |= VOICE_PORTAMENTO;
                }
            }
            sv->cFlags24 |= VOICE_UPDATED;
        }
        if (sv->cFlags24 & VOICE_START_SAMPLE) {
            sv->cFlags24 &= ~VOICE_START_SAMPLE;
            sv->cFlags24 |= VOICE_PLAYING;
            sound_hw_start_voice(i);
        }
        if ((sv->cFlags24 & VOICE_RELEASE) && !(sv->cFlags24 & VOICE_PEDAL_DOWN)) {
            sv->cFlags24 &= ~(VOICE_RELEASE | VOICE_PLAYING);
            sound_hw_release_hold(i);
        }
        if (sv->age28 != 0) {
            sv->age28 -= sv->ageSpeed2C * sound_tick_step;
            if ((s32)sv->age28 < 0) {
                sv->age28 = 0;
            }
        }
        if (sv->cFlags24 & VOICE_PAN_SWEEP) {
            sv->panning38 = sv->panTargetAC - (sv->panTimeA8 >> 8) * sv->panDelta3C;
            sv->panning38 = sv->panning38 < 0 ? 0 : (u32)sv->panning38 > 0x7F0000U ? 0x7F0000U : sv->panning38;
            sv->panTimeA8 -= sound_tick_step;
            if (sv->panTimeA8 == 0) {
                sv->cFlags24 &= ~VOICE_PAN_SWEEP;
                sv->panning38 = sv->panTargetAC;
            }
        }
        note_pitch = (sv->curNote50 << 16) + (sv->curDetuneC0 << 16) / 100;
        if (sv->cFlags24 & (VOICE_TRACK_PITCH_WHEEL | VOICE_PLAYING)) {
            if (sv->midi4A != NO_CHANNEL) {
                pitch_bend = sound_synth_get_pitch_bend_input(sv);
                sv->pbLastB8 = pitch_bend;
                goto apply_pitch_bend;
            }
        } else {
            pitch_bend = sv->pbLastB8;
        apply_pitch_bend:
            if (pitch_bend != PITCH_BEND_CENTER) {
                pitch_bend -= PITCH_BEND_CENTER;
                if (pitch_bend < 0) {
                    note_pitch += sv->pbLowerKeyRange6A * pitch_bend * 8;
                } else {
                    note_pitch += sv->pbUpperKeyRange6B * pitch_bend * 8;
                }
            }
        }
        if (sv->cFlags24 & VOICE_VIBRATO) {
            vibrato_range = (sv->vibKeyRange78 << 8) + (sv->vibCentRange79 << 8) / 100;
            if ((sv->cFlags24 & VOICE_VIBRATO_BY_MODULATION) && sv->midi4A != NO_CHANNEL) {
                vibrato = ((sound_synth_get_modulation_input(sv) >> 7) * sv->vibCurOffset74) >> 7;
            } else {
                vibrato = sv->vibCurOffset74;
            }
            note_pitch += (vibrato_range * vibrato) >> 4;
        }
        pitch = sv->sweepOff40[1] + (sv->sweepOff40[0] + sound_synth_key_to_pitch((struct VoicePrefix *)sv, sound_synth_apply_portamento((struct RampState *)sv, note_pitch)));
        if (sv->midi4A != NO_CHANNEL) {
            pitch = (sound_synth_get_doppler_input(sv) * (pitch >> 16)) >> 13;
        } else {
            pitch >>= 16;
        }
        span = pitch;   /* span serves as the pitch's temporary here */
        sound_hw_set_pitch(i, span);
        span = 0;
        if (sv->cFlags24 & VOICE_VOLUME_SWEEP) {
            sv->volume30 = sv->envTarget88 - (sv->envTimeB4 >> 8) * sv->envDelta84;
            sv->volume30 = sv->volume30 < 0 ? 0 : (u32)sv->volume30 > 0x7F0000U ? 0x7F0000U : sv->volume30;
            sv->envTimeB4 -= sound_tick_step;
            if ((s32)sv->envTimeB4 <= 0) {
                sv->cFlags24 &= ~VOICE_VOLUME_SWEEP;
                sv->volume30 = sv->envTarget88;
            }
        }
        vol = sv->volume30;
        pan = sv->panning38;
        midi = sv->midi4A;
        vol = (VOLUME_FADERS[sv->vGroupBE].pauseVolume >> 16) * (vol >> 7);
        vol = (VOLUME_FADERS[sv->vGroupBE].volume >> 16) * (vol >> 7);
        vol = (VOLUME_FADERS[sv->fxFlag4C ? FADER_SFX : FADER_MUSIC].volume >> 16) * (vol >> 7);
        if (sv->track2F != 0xFF) {
            vol = sound_track_volumes[sv->track2F] * (vol >> 7);
        }
        if (sound_mono == 0) {
            if ((span = midi) != NO_CHANNEL) {
                vol = (sound_synth_get_volume_input(sv) >> 7) * (vol >> 7);
                if (pan != 0x800000) {
                    pan += ((sound_synth_get_pan_input(sv) >> 7) - 0x40) << 16;
                    pan = pan < 0 ? 0 : pan > 0x7F0000 ? 0x7F0000 : pan;
                }
                span = sound_synth_get_span_input(sv) << 9;
                if (span > 0x7F0000) {
                    span = 0x7F0000;
                }
            } else {
                span = 0;
            }
        } else {
            if (midi != NO_CHANNEL) {
                vol = (sound_synth_get_volume_input(sv) >> 7) * (vol >> 7);
            }
            pan = 0x400000;
        }
        if (midi != NO_CHANNEL) {
            reverb = ((sound_synth_get_reverb_input(sv) >> 7) * vol) >> 7;
            if (reverb > 0x7F0000) {
                reverb = 0x7F0000;
            }
            reverb = (reverb >> 7) * sv->revVolScale99;
            if (reverb > 0x7F0000) {
                reverb = 0x7F0000;
            }
            reverb = (sv->revVolOffset9A << 16) + reverb;
            if (reverb > 0x7F0000) {
                reverb = 0x7F0000;
            }
        }
        else {
            reverb = 0;
        }
        sv->curOutputVolumeC2 = vol >> 8;
        sound_hw_set_volume(i, vol, pan, span, reverb);
    }
    for (j = 0; j < FADER_COUNT; j++) {
        if (VOLUME_FADERS[j].time != 0) {
            VOLUME_FADERS[j].volume = VOLUME_FADERS[j].target - (VOLUME_FADERS[j].time >> 8) * VOLUME_FADERS[j].delta;
            if (VOLUME_FADERS[j].volume < 0) {
                VOLUME_FADERS[j].volume = 0;
            }
            VOLUME_FADERS[j].time -= sound_tick_step;
            if (VOLUME_FADERS[j].time <= 0) {
                VOLUME_FADERS[j].time = 0;
                VOLUME_FADERS[j].volume = VOLUME_FADERS[j].target;
                switch (VOLUME_FADERS[j].seqMode) {
                case FADE_THEN_STOP:
                    sound_seq_stop_unlocked(VOLUME_FADERS[j].seqId);
                    break;
                case FADE_THEN_PAUSE:
                    sound_seq_pause_unlocked(VOLUME_FADERS[j].seqId);
                    break;
                case FADE_THEN_MUTE:
                    sound_seq_set_track_mask_unlocked(VOLUME_FADERS[j].seqId, 0, 0);
                    break;
                }
            }
        }
        if (VOLUME_FADERS[j].pauseTime != 0) {
            VOLUME_FADERS[j].pauseVolume = VOLUME_FADERS[j].pauseTarget - (VOLUME_FADERS[j].pauseTime >> 8) * VOLUME_FADERS[j].pauseDelta;
            if (VOLUME_FADERS[j].pauseVolume < 0) {
                VOLUME_FADERS[j].pauseVolume = 0;
            }
            VOLUME_FADERS[j].pauseTime -= sound_tick_step;
            if (VOLUME_FADERS[j].pauseTime <= 0) {
                VOLUME_FADERS[j].pauseTime = 0;
                VOLUME_FADERS[j].pauseVolume = VOLUME_FADERS[j].pauseTarget;
            }
        }
    }
    sound_hw_commit_voices();
}

/* One library tick: the tick's length (sound_tick_step, ms in 24.8 fixed point, from the samples a tick renders), then
 * the voice update. */
void sound_synth_tick(void)
{
    if (sound_samples_per_tick != 0) {
        sound_tick_step = (u32)((s32)((u32)sound_samples_per_tick * 32000U) / sound_synth_frequency) << 3;
        sound_synth_update_voices();
    }
}

/* Starts the effect with this id from the effect tables: volume and pan 255 take the effect's own. The caller holds
 * the library lock. Returns the handle, or VOICE_NONE. */
u32 sound_start_sfx_unlocked(u16 identifier, u8 volume, u8 pan)
{
    Preset *preset;
    u32 result;
    result = VOICE_NONE;
    preset = sound_find_sfx(identifier);
    if (preset != 0) {
        if (volume == 255) volume = preset->volume;
        if (pan == 255) pan = preset->pan;
        result = sound_synth_start_note(((u32)preset->macro_id << 16) |
            ((u32)preset->priority << 8) | preset->max_voices,
            preset->key | 128, volume, pan, 255, 255, 0, 255,   /* an effect: no MIDI channel or set, step 0, no section */
            preset->volume_group, 0);
    }
    return result;
}

/* Sets an effect's pan (MIDI controller 10) on every voice of its chain. The caller holds the library lock. Returns 0,
 * or -1 if the handle is no longer valid. The span, doppler, pitch bend and volume setters below are the same. */
int sound_set_sfx_pan_unlocked(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != VOICE_NONE) {
        index = identifier & VOICE_INDEX_MASK;
        if (sound_synth_voices[index].identifier60 == identifier) {
            result = 0;
            if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                sound_midi_set_controller(CTRL_PAN, index, sound_synth_voices[index].setupMidiSet55, value);
            } else {
                sound_midi_set_controller(CTRL_PAN, index, sound_synth_voices[index].midiSet4B, value);
            }
            identifier = sound_synth_voices[index].child10;
        } else {
            return result;
        }
    }
    return result;
}

int sound_set_sfx_span_unlocked(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != VOICE_NONE) {
        index = identifier & VOICE_INDEX_MASK;
        if (sound_synth_voices[index].identifier60 == identifier) {
            result = 0;
            if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                sound_midi_set_controller(CTRL_SPAN, index, sound_synth_voices[index].setupMidiSet55, value);
            } else {
                sound_midi_set_controller(CTRL_SPAN, index, sound_synth_voices[index].midiSet4B, value);
            }
            identifier = sound_synth_voices[index].child10;
        } else {
            return result;
        }
    }
    return result;
}

int sound_set_sfx_doppler_unlocked(u32 identifier, u16 value)
{
    int result;
    u32 index;
    u8 low;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != VOICE_NONE) {
        index = identifier & VOICE_INDEX_MASK;
        if (sound_synth_voices[index].identifier60 == identifier) {
            low = value & 127;
            if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                sound_midi_set_controller(CTRL_DOPPLER, index, sound_synth_voices[index].setupMidiSet55, value >> 7);
                sound_midi_set_controller(CTRL_DOPPLER_LOW, index, sound_synth_voices[index].setupMidiSet55, low);
            } else {
                sound_midi_set_controller(CTRL_DOPPLER, index, sound_synth_voices[index].midiSet4B, value >> 7);
                sound_midi_set_controller(CTRL_DOPPLER_LOW, index, sound_synth_voices[index].midiSet4B, low);
            }
            result = 0;
            identifier = sound_synth_voices[index].child10;
        } else {
            return result;
        }
    }
    return result;
}

int sound_set_sfx_pitch_bend_unlocked(u32 identifier, u16 value)
{
    int result;
    u32 index;
    u8 low;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != VOICE_NONE) {
        index = identifier & VOICE_INDEX_MASK;
        if (sound_synth_voices[index].identifier60 == identifier) {
            low = value & 127;
            if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                sound_midi_set_controller(CTRL_PITCH_BEND, index, sound_synth_voices[index].setupMidiSet55, value >> 7);
                sound_midi_set_controller(CTRL_PITCH_BEND_LOW, index, sound_synth_voices[index].setupMidiSet55, low);
            } else {
                sound_midi_set_controller(CTRL_PITCH_BEND, index, sound_synth_voices[index].midiSet4B, value >> 7);
                sound_midi_set_controller(CTRL_PITCH_BEND_LOW, index, sound_synth_voices[index].midiSet4B, low);
            }
            result = 0;
            identifier = sound_synth_voices[index].child10;
        } else {
            return result;
        }
    }
    return result;
}

/* Copies the volume, pan, reverb, pitch bend and doppler controllers of one voice to another. */
void sound_synth_copy_controllers(struct VoiceState *destination, struct VoiceState *source)
{
    sound_midi_copy_voice_controller(CTRL_VOLUME, destination, source);
    sound_midi_copy_voice_controller(CTRL_PAN, destination, source);
    sound_midi_copy_voice_controller(CTRL_REVERB, destination, source);
    sound_midi_copy_voice_controller(CTRL_PITCH_BEND, destination, source);
    sound_midi_copy_voice_controller(CTRL_DOPPLER, destination, source);
}

int sound_set_sfx_volume_unlocked(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != VOICE_NONE) {
        index = identifier & VOICE_INDEX_MASK;
        if (sound_synth_voices[index].identifier60 == identifier) {
            result = 0;
            if (sound_synth_voices[index].flags24 & VOICE_STARTING) {
                sound_midi_set_controller(CTRL_VOLUME, index, sound_synth_voices[index].setupMidiSet55, value);
            } else {
                sound_midi_set_controller(CTRL_VOLUME, index, sound_synth_voices[index].midiSet4B, value);
            }
            identifier = sound_synth_voices[index].child10;
        } else {
            return result;
        }
    }
    return result;
}

/* Asks every voice of a handle's chain to key off (their macros decide what that does). The caller holds the library
 * lock. Returns 0, or -1 if the library is not started or the handle is not valid. */
int sound_key_off_unlocked(u32 key)
{
    u32 index;
    int result;
    result = -1;
    if (sound_initialized) {
        key = sound_resolve_handle(key);
        while (key != VOICE_NONE) {
            index = key & VOICE_INDEX_MASK;
            if (sound_synth_voices[index].identifier60 == key) {
                sound_synth_voices[index].flags24 |= VOICE_KEY_OFF;
                result = 0;
            }
            key = sound_synth_voices[index].child10;
        }
    }
    return result;
}

/* The output volume (curOutputVolumeC2) of a sound's first voice, or 0 if the handle is not valid or the voice has not
 * run yet. */
u16 sound_get_output_volume(u32 key)
{
    int identifier;
    identifier = sound_resolve_handle(key);
    if (identifier != -1) {
        if (sound_synth_voices[identifier & VOICE_INDEX_MASK].identifier60 == (u32)identifier &&
            !(sound_synth_voices[identifier & VOICE_INDEX_MASK].flags24 & VOICE_STARTING)) {
            return sound_synth_voices[identifier & VOICE_INDEX_MASK].curOutputVolumeC2;
        }
    }
    return 0;
}

/* Fades volume groups to volume (0-127) over time ms (at least 1). vGroup selects them: 255 every group of types 0 and
 * 1, 252 every group of types 2 and 3, 250, 251, 253, 254 every group of type 2, 3, 0, 1; any other is one fader, whose
 * fade then does seqMode (FADE_THEN_STOP, _PAUSE, _MUTE) to the song seqId. */
void sound_fade_volume_group_unlocked(u8 volume, u16 time, u8 vGroup, u8 seqMode, u32 seqId)
{
    u32 duration;
    u32 i;
    u8 type;

    if (time == 0) {
        ++time;
    }
    duration = time;
    sound_ms_to_time(&duration);
    switch (vGroup) {
    case 255:
        for (i = 0; i < FADER_COUNT; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 0 || ((MasterFader *)sound_volume_faders)[i].type == 1) {
                ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].time = duration;
                ((MasterFader *)sound_volume_faders)[i].delta = (s32)(((MasterFader *)sound_volume_faders)[i].target - ((MasterFader *)sound_volume_faders)[i].volume) / time;
                ((MasterFader *)sound_volume_faders)[i].seqId = 0xFFFFFFFFU;
            }
        }
        return;
    case 252:
        for (i = 0; i < FADER_COUNT; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 2 || ((MasterFader *)sound_volume_faders)[i].type == 3) {
                ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].time = duration;
                ((MasterFader *)sound_volume_faders)[i].delta = (s32)(((MasterFader *)sound_volume_faders)[i].target - ((MasterFader *)sound_volume_faders)[i].volume) / time;
                ((MasterFader *)sound_volume_faders)[i].seqId = 0xFFFFFFFFU;
            }
        }
        return;
    case 250: type = 2; goto setup_type;
    case 251: type = 3; goto setup_type;
    case 253: type = 0; goto setup_type;
    case 254: type = 1; goto setup_type;
    setup_type:
        for (i = 0; i < FADER_COUNT; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == type) {
                ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].time = duration;
                ((MasterFader *)sound_volume_faders)[i].delta = (s32)(((MasterFader *)sound_volume_faders)[i].target - ((MasterFader *)sound_volume_faders)[i].volume) / time;
                ((MasterFader *)sound_volume_faders)[i].seqId = 0xFFFFFFFFU;
            }
        }
        return;
    default:
        i = vGroup;
        ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
        ((MasterFader *)sound_volume_faders)[i].time = duration;
        ((MasterFader *)sound_volume_faders)[i].delta = (s32)(((MasterFader *)sound_volume_faders)[i].target - ((MasterFader *)sound_volume_faders)[i].volume) / time;
        ((MasterFader *)sound_volume_faders)[i].seqMode = seqMode;
        ((MasterFader *)sound_volume_faders)[i].seqId = seqId;
        return;
    }
}

/* Whether volume group index is fading out (in use, a fade under way, the volume going down). */
int sound_is_group_fading_out(u8 index)
{
    ChannelState *state;
    state = &sound_volume_faders[index];
    if (state->type != FADER_UNUSED && state->time != 0 && (int)state->delta < 0) return 1;
    return 0;
}

/* Fades the pause volumes of volume groups, selected as for sound_fade_volume_group_unlocked, to volume over time ms. */
void sound_fade_pause_volume_unlocked(u8 volume, u16 time, u8 group)
{
    u32 i;
    u32 duration;
    u8 type;

    if (time == 0) {
        ++time;
    }
    duration = time;
    sound_ms_to_time(&duration);
    switch (group) {
    case 255:
        for (i = 0; i < FADER_COUNT; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 0 || ((MasterFader *)sound_volume_faders)[i].type == 1) {
                ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].pauseTime = duration;
                ((MasterFader *)sound_volume_faders)[i].pauseDelta = (s32)(((MasterFader *)sound_volume_faders)[i].pauseTarget - ((MasterFader *)sound_volume_faders)[i].pauseVolume) / time;
            }
        }
        return;
    case 252:
        for (i = 0; i < FADER_COUNT; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 2 || ((MasterFader *)sound_volume_faders)[i].type == 3) {
                ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].pauseTime = duration;
                ((MasterFader *)sound_volume_faders)[i].pauseDelta = (s32)(((MasterFader *)sound_volume_faders)[i].pauseTarget - ((MasterFader *)sound_volume_faders)[i].pauseVolume) / time;
            }
        }
        return;
    case 250:
        type = 2;
        goto setup_type;
    case 251:
        type = 3;
        goto setup_type;
    case 253:
        type = 0;
        goto setup_type;
    case 254:
        type = 1;
        goto setup_type;
    setup_type:
        for (i = 0; i < FADER_COUNT; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == type) {
                ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].pauseTime = duration;
                ((MasterFader *)sound_volume_faders)[i].pauseDelta = (s32)(((MasterFader *)sound_volume_faders)[i].pauseTarget - ((MasterFader *)sound_volume_faders)[i].pauseVolume) / time;
            }
        }
        return;
    default:
        i = group;
        ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
        ((MasterFader *)sound_volume_faders)[i].pauseTime = duration;
        ((MasterFader *)sound_volume_faders)[i].pauseDelta = (s32)(((MasterFader *)sound_volume_faders)[i].pauseTarget - ((MasterFader *)sound_volume_faders)[i].pauseVolume) / time;
        return;
    }
}

/* Sets volume group channel's type. sound_volume_fader_types is the faders' type bytes (fader 0's at +0x14, a stride
 * of 40 bytes): [channel][0] is fader channel's type. */
void sound_set_group_type(unsigned char group, unsigned char type)
{
    if (sound_initialized) {
        sound_volume_fader_types[group][0] = type;
    }
}

/* The synth's start-up at an output rate (configuration, Hz): tempo 120 for the effects, stereo, every voice free
 * with its defaults, every fader unused at full volume but the masters and the song players' and effects' default
 * groups, the handles, the voice allocation, the global variables and the channel owners reset. */
void sound_synth_init(u32 frequency)
{
    int i;
    sound_synth_frequency = frequency;
    sound_synth_unread_setting = 10240;
    sound_synth_set_tempo(120,255);
    sound_mono = 0;
    for (i=0;i<PC_MX_LOOP(32);i++) {
        sound_synth_voices[i].identifier60 = VOICE_NONE;
        sound_synth_voices[i].addr00 = 0;
        sound_synth_voices[i].flags24 = 0;
        sound_synth_voices[i].age28 = 0;
        sound_synth_voices[i].priority2E = 0;
        sound_synth_voices[i].field68 = 0;
        sound_synth_voices[i].midi4A = NO_CHANNEL;
        sound_synth_voices[i].volume30 = 0;
        sound_synth_voices[i].revVolScale99 = 128;
        sound_synth_voices[i].revVolOffset9A = 0;
        sound_synth_voices[i].panning38 = 0x3F0000;
        sound_synth_voices[i].sweepOff40[0] = 0;
        sound_synth_voices[i].sweepOff40[1] = 0;
        sound_synth_voices[i].sweepNum48[0] = 0;
        sound_synth_voices[i].sweepNum48[1] = 0;
        sound_synth_voices[i].blockedBD = 0;
        sound_synth_voices[i].vGroupBE = FADER_SONG_FIRST;
        sound_synth_voices[i].lfoPeriod16C = 0;
        sound_synth_voices[i].lfoValue170 = 0;
        sound_synth_voices[i].lfoPeriod178 = 0;
        sound_synth_voices[i].lfoValue17C = 0;
        sound_synth_voices[i].portTime8C = 100;
        sound_synth_voices[i].portType98 = 0;
    }
    for (i=0;i<FADER_COUNT;i++) {
        sound_volume_faders[i].volume = 0;
        sound_volume_faders[i].target = 0;
        sound_volume_faders[i].time = 0;
        sound_volume_faders[i].type = FADER_UNUSED;
        sound_volume_faders[i].pauseVolume = 0x7F0000;
    }
    sound_volume_faders[31].type = 1;
    for (i=0;i<8;i++) sound_volume_faders[i+FADER_SONG_FIRST].type = 0;
    sound_volume_faders[FADER_MUSIC].volume = 0x7F0000;
    sound_volume_faders[FADER_SFX].volume = 0x7F0000;
    sound_init_handles();
    sound_init_voice_allocation();
    for (i=0;i<16;i++) {
        sound_global_variables[i] = 0;
    }
    sound_synth_init_channel_owners();
}
