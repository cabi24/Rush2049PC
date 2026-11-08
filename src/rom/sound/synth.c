/* GENERATED ROM-aligned TU — segment 0x1a660 (rom/lib_1a660)
 * layout map 91b72acb516b2291b9dbf568e33b80bbc7fb9c8413206c2c73c661eb5c01ac02.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-11 — sound_synth_set_tempo
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int sound_tick_rates[];
void sound_synth_set_tempo(unsigned int value, unsigned char index)
{
    if (index == 255) {
        index = 8;
    }
    sound_tick_rates[index] = (value << 3) * 1536 / 240;
}

/* PROMOTED 2026-10-04 — sound_synth_get_tick_rate
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned int sound_tick_rates[];
unsigned int sound_synth_get_tick_rate(unsigned char *state)
{
    return sound_tick_rates[state[75] == 255 ? 8 : state[75]];
}

/* PROMOTED 2026-10-11 — sound_synth_apply_portamento
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct RampState { u8 unknown00[36]; u32 flags24; u8 unknown28[100]; u32 start8C; u32 end90; u32 current94; } RampState;
#pragma pack(0)
extern u32 sound_tick_step;
u32 sound_synth_apply_portamento(RampState *state, u32 target)
{
    s32 duration;
    u32 previous;
    u32 start;
    u32 end;
    if (state->flags24 & 0x800) {
        end = state->end90;
        start = state->start8C;
        duration = (end - start) >> 8;
        if (duration > 0) {
            previous = state->current94;
            state->current94 += (s32)(sound_tick_step * (u32)((s32)(target - state->current94) >> 8)) / duration;
            if ((previous < target && state->current94 < target) ||
                (previous > target && state->current94 > target)) {
                target = state->current94;
                state->start8C = start + sound_tick_step;
            } else {
                state->start8C = end;
            }
        }
    }
    return target;
}

/* PROMOTED 2026-10-04 — sound_synth_start_portamento
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct VoiceState { u8 unknown00[36]; u32 flags; u8 unknown28[34]; u8 channel; u8 set; u8 unknown4C[64]; u32 current8C; u32 stored90; u32 fixed94; u8 mode98; u8 unknown99[40]; u8 lastC1; } VoiceState;
#pragma pack(0)
extern void sound_midi_set_portamento_state(u8, u8, u8);
void sound_synth_start_portamento(VoiceState *state)
{
    if (!(state->flags & 0x80000)) {
        if (state->mode98 == 1) {
            if (!(state->flags & 0x2000)) state->current8C = 0;
            else state->current8C = state->stored90;
        } else {
            state->current8C = state->stored90;
        }
        state->fixed94 = (u32)state->lastC1 << 16;
    }
    if (state->channel != 255) sound_midi_set_portamento_state(state->channel, state->set, 1);
}

/* PROMOTED 2026-10-04 — sound_synth_retrigger_legato
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
/* Array stride 0x1A0; expose the selector at +0x55 and value at +0xC2
 * without changing the field names used by the accepted body below.
 * names the fields sound_synth_init initializes, carved from the
 * unknown ranges; every offset and the 0x1A0 size are unchanged. */
typedef struct SynthVoiceLegatoView { u32 command00;u8 unknown04[12];u32 child,parent;u8 unknown18[12]; u32 flags;u32 age28;u8 unknown2C[2];u8 channel2E,unknown2F;u32 volume30;u8 unknown34[4];u32 panning38;u8 unknown3C[4];u32 words40[2];u8 status48,status49;u8 channel,set;u8 unknown4C[2]; u16 base_key,key;u8 unknown52[3];u8 channel55;u8 unknown56[10];u32 identifier; u8 unknown64[4];u16 loop68;u8 unknown6A[34];u32 glide;u8 unknown90[4];u32 pitch; u8 state98,depth99,flag9A;u8 unknown9B[34];u8 activeBD,groupBE,unknownBF;s8 cents;u8 original_key;u16 valueC2;u8 unknownC4[168];u32 word16C;u16 half170;u8 unknown172[6];u32 word178;u16 half17C;u8 unknown17E[34]; } SynthVoiceLegatoView;
#pragma pack(0)
/* On the PC the voices are PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9), seen here in this file's view; PC_MX_LOOP(32) is the run's voices where the N64 sets up
 * its 32 (docs/port/AUDIO.md 10). */
#define sound_synth_voices (*(SynthVoiceLegatoView (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)
#define PC_MX_LOOP(n) PC_MX_SLOTS
extern u8 sound_voice_count;
extern u8 sound_hw_is_voice_active(u32);
extern void sound_unlink_voice(SynthVoiceLegatoView *);
extern u32 sound_new_handle(SynthVoiceLegatoView *);
extern void sound_synth_claim_channel(SynthVoiceLegatoView *);
extern void sound_midi_set_last_key(u8,u8,u8);
u32 sound_synth_retrigger_legato(u8 key,u8 channel,u8 set)
{
    u32 i,result,previous;
    SynthVoiceLegatoView *state,*last;
    result=0xFFFFFFFFU;
    for (i=0,state=sound_synth_voices;i<sound_voice_count;i++,state++) {
        if (state->identifier!=0xFFFFFFFFU && state->channel==channel && state->set==set &&
            (state->flags&16) && (!(state->flags&8) || (state->flags&0x40000000)) &&
            sound_hw_is_voice_active(i)) {
            last=state;
            state->pitch=((u32)state->key<<16)+(state->cents*65536)/100;
            state->original_key=state->key;
            state->key=key+(state->key&255)-(u8)state->base_key;
            state->base_key=key;
            state->cents=0;
            state->glide=0;
            state->flags|=0x80800;
            sound_unlink_voice(&sound_synth_voices[i]);
            if (result==0xFFFFFFFFU) {
                state->child=0xFFFFFFFFU;
                state->parent=0xFFFFFFFFU;
                result=sound_new_handle(&sound_synth_voices[i]);
                previous=state->identifier;
            } else {
                sound_synth_voices[(previous&255)].child=state->identifier;
                state->parent=previous;
                previous=state->identifier;
            }
        }
    }
    if (result!=0xFFFFFFFFU) {
        sound_synth_claim_channel(last);
        sound_midi_set_last_key(last->channel,last->set,(u8)last->key);
    }
    return result;
}

/* PROMOTED 2026-10-11 — sound_synth_try_legato
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_midi_get_portamento_state(u8, u8);
extern u32 sound_synth_retrigger_legato(u8, u8, u8);
u32 sound_synth_try_legato(u8 key, u8 channel, u8 set)
{
    u32 identifier;
    if (channel != 255 && sound_midi_get_portamento_state(channel, set) != 0) {
        identifier = sound_synth_retrigger_legato(key & 127, channel, set);
        if (identifier != 0xFFFFFFFFU) {
            return identifier;
        }
    }
    return 0xFFFFFFFFU;
}

/* PROMOTED 2026-10-11 — sound_synth_start_layer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
#pragma pack(1)
typedef struct LayerEntry { u16 id; u8 low; u8 high; s8 transpose; u8 volume; s16 priority; u8 panning; u8 unknown09[3]; } LayerEntry;
#pragma pack(0)
extern void *sound_find_layer(u16, u16 *);
extern u32 sound_synth_try_legato(u8, u8, u8);
extern u32 sound_macro_start(u32, u16, u8, u8, u8, u8, u8, u16, u16, u8, u8);
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
    u8 scaled;
    result = 0xFFFFFFFFU;
    layers = sound_find_layer(packed >> 16, &count);
    if (layers != 0) {
        for (i = 0; i < count; i++) {
            layer = &layers[i];
            if (layer->id != 0xFFFF && layer->low <= (key & 127) && layer->high >= (key & 127)) {
            note = layer->transpose + (key & 127);
            note = note > 127 ? 127 : note < 0 ? 0 : note;
            current = sound_synth_try_legato(note, channel, set);
            if (current != 0xFFFFFFFFU) return current;
            if ((layer->panning & 128) == 0) {
                pan = layer->panning - 64;
                pan += panning;
                pan = pan < 0 ? 0 : pan > 127 ? 127 : pan;
            } else pan = 128;
            scaled = (layer->volume * volume) / 127;
            priority = ((packed >> 8) & 255) + layer->priority;
            priority = priority > 255 ? 255 : priority < 0 ? 0 : priority;
            nextPacked = (packed & 255) | ((u32)priority << 8) | ((u32)layer->id << 16);
            switch (layer->id & 0xC000) {
            case 0:
                if (result == 0xFFFFFFFFU) {
                    previous = sound_macro_start(nextPacked, allocation, note | (key & 128), scaled,
                        pan, channel, set, offset, section, 0, group);
                    if (previous != 0xFFFFFFFFU) result = sound_new_handle(&sound_synth_voices[previous & 255]);
                } else {
                    current = sound_macro_start(nextPacked, allocation, note | (key & 128), scaled,
                        pan, channel, set, offset, section, 0, group);
                    if (current != 0xFFFFFFFFU) {
                        sound_synth_voices[previous & 255].child = current;
                        sound_synth_voices[current & 255].parent = previous;
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

/* PROMOTED 2026-10-11 — sound_synth_start_note
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct Keymap { u16 id; s8 transpose; u8 panning; s16 priority; u8 unknown06[2]; } Keymap;
#pragma pack(0)
extern Keymap *sound_find_keymap(u16);
extern u32 sound_synth_try_legato(u8, u8, u8);
extern u32 sound_macro_start(u32, u16, u8, u8, u8, u8, u8, u16, u16, u8, u8);
extern u32 sound_synth_start_layer(u32, u16, u8, u8, u8, u8, u8, u16, u16, u8);
u32 sound_synth_start_note(u32 packed, u8 key, u8 volume, u8 panning,
                 u8 channel, u8 set, u16 offset, u16 section, u8 group, s16 priorityOffset)
{
    Keymap *keymap;
    u16 id;
    int pan;
    int pan_sum;
    int priority;
    int note;
    id = packed >> 16;
    priority = (int)((packed >> 8) & 255) + priorityOffset;
    priority = priority < 0 ? 0 : priority > 255 ? 255 : priority;
    packed = (packed & 0xFFFF00FFU) | ((u32)priority << 8);
    switch (id & 0xC000) {
    case 0:
        priority = sound_synth_try_legato(key, channel, set);
        if (priority != 0xFFFFFFFFU) return priority;
        return sound_macro_start(packed, packed >> 16, key, volume, panning, channel, set, offset, section, 1, group);
    case 0x4000:
        keymap = sound_find_keymap(id);
        if (keymap != 0) {
            if ((pan = keymap[(u8)(key & 127)].id) != 0xFFFF) {
                if ((keymap[(u8)(key & 127)].panning & 128) == 0) {
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
                packed |= (u32)pan << 16;
                if ((pan & 0xC000) == 0) {
                    priority = sound_synth_try_legato(note, channel, set);
                    if (priority != 0xFFFFFFFFU) return priority;
                    return sound_macro_start(packed, id, note | (key & 128), volume, panning, channel, set, offset, section, 1, group);
                }
                return sound_synth_start_layer(packed, id, note | (key & 128), volume, panning, channel, set, offset, section, group);
            }
        }
        break;
    case 0x8000:
        return sound_synth_start_layer(packed, packed >> 16, key, volume, panning, channel, set, offset, section, group);
    }
    return 0xFFFFFFFFU;
}

/* PROMOTED 2026-10-11 — sound_synth_key_to_pitch
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct VoicePrefix { u8 unknown00[92]; u32 pitch5C; } VoicePrefix;
#pragma pack(0)
extern u32 sound_note_to_pitch(u8, u32);
extern u16 sound_pitch_up_semitone(u16);
u32 sound_synth_key_to_pitch(VoicePrefix *state, u32 key)
{
    u32 fraction;
    u32 result;
    result = sound_note_to_pitch(key >> 16, state->pitch5C) << 16;
    fraction = key & 65535;
    if (fraction != 0) {
        result += (sound_pitch_up_semitone(result >> 16) - (result >> 16)) * fraction;
    }
    return result;
}

typedef struct ChannelState {u32 word00,word04,flags08,pending0C,unknown10;u8 kind14;u8 unknown15[3];u32 value18;u8 unknown1C[12];} ChannelState;
extern ChannelState sound_volume_faders[32];
/* PROMOTED 2026-10-12 — sound_synth_update_voices
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
struct RampState;
struct VoicePrefix;
struct VoiceState;
struct SynthVoiceLegatoView;
#pragma pack(1)
typedef struct SynthLfo { u32 time; u32 period; s16 value; s16 lastValue; } SynthLfo;
/* PC: +0 is the voice's macro start, a 32-bit window address (macros.c) */
typedef struct SynthVoice { u32 addr00; u8 unknown04[0x20]; u32 cFlags24; u32 age28; u16 ageSpeed2C; u8 unknown2E; u8 track2F; s32 volume30; u8 unknown34[4]; s32 panning38; s32 panDelta3C; s32 sweepOff40[2]; u8 sweepNum48[2]; u8 midi4A; u8 midiSet4B; u8 fxFlag4C; u8 unknown4D[3]; u16 curNote50; u8 unknown52[0x18]; u8 pbLowerKeyRange6A; u8 pbUpperKeyRange6B; u32 vibCurTime6C; u32 vibPeriod70; s32 vibCurOffset74; u8 vibKeyRange78; u8 vibCentRange79; u8 unknown7A[2]; s32 sweepAdd7C[2]; s32 envDelta84; s32 envTarget88; u8 unknown8C[0xD]; u8 revVolScale99; u8 revVolOffset9A; u8 unknown9B[0xD]; u32 panTimeA8; s32 panTargetAC; u8 sweepCntB0[2]; u8 unknownB2[2]; u32 envTimeB4; u16 pbLastB8; u8 unknownBA[3]; u8 blockedBD; u8 vGroupBE; u8 unknownBF; s8 curDetuneC0; u8 unknownC1; u16 curOutputVolumeC2; u8 unknownC4[0xA4]; SynthLfo lfo168[2]; s16 localVars180[16]; } SynthVoice;
#pragma pack(0)
typedef struct SynthFader { s32 volume; s32 target; s32 delta; s32 time; u32 seqId; u8 type; u8 seqMode; u8 unknown16[2]; s32 pauseVolume; s32 pauseTarget; s32 pauseDelta; s32 pauseTime; } SynthFader;
extern u8 sound_track_volumes[];
extern u8 sound_mono;
extern u32 sound_synth_apply_portamento(struct RampState *, u32);
extern void sound_synth_start_portamento(struct VoiceState *);
extern u32 sound_synth_key_to_pitch(struct VoicePrefix *, u32);
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
#define VOLUME_FADERS ((SynthFader *)sound_volume_faders)
void sound_synth_update_voices(void)
{
    SynthVoice *sv;
    s32 aux;
    s32 i;
    s32 j;
    u32 ccents;
    s32 pbend;
    s32 voff;
    s32 span;
    u32 cpitch;
    s32 vol;
    s32 pan;
    s32 vrange;
    u8 midi;

    for (i = 0, sv = (SynthVoice *)sound_synth_voices; i < sound_voice_count; i++, sv++) {
        if (sv->addr00 != 0) {
            sound_macro_run(sv);
        } else if (!sound_hw_is_voice_active(i)) {
            continue;
        }
        if (sv->addr00 == 0) pc_mx_note_macroless(i); if (sv->blockedBD != 0) {
            continue;
        }
        for (j = 0; j < 2; j++) {
            if (sv->lfo168[j].period != 0) {
                sv->lfo168[j].time += sound_tick_step;
                sv->lfo168[j].value = sound_sine(((sv->lfo168[j].time % sv->lfo168[j].period) * 16) / (sv->lfo168[j].period >> 8));
            }
        }
        if (sv->cFlags24 & 0x4000) {
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
        if (sv->midi4A != 0xFF) {
            if (sound_synth_get_pedal_input(sv) <= 0x1F80) {
                sv->cFlags24 &= ~0x40000000;
            } else {
                sv->cFlags24 |= 0x40000000;
            }
        } else {
            sv->cFlags24 &= ~0x40000000;
        }
        if (!(sv->cFlags24 & 0x1000)) {
            if (sv->midi4A != 0xFF) {
                if (sound_synth_get_portamento_input(sv) <= 0x1F80) {
                    sv->cFlags24 &= ~0x800;
                    sound_midi_set_portamento_state(sv->midi4A, sv->midiSet4B, 0);
                } else {
                    if (!(sv->cFlags24 & 0x800)) {
                        sound_synth_start_portamento((struct VoiceState *)sv);
                    }
                    sv->cFlags24 |= 0x800;
                }
            }
            sv->cFlags24 |= 0x2000;
        }
        if (sv->cFlags24 & 0x20) {
            sv->cFlags24 &= ~0x20;
            sv->cFlags24 |= 0x10;
            sound_hw_start_voice(i);
        }
        if ((sv->cFlags24 & 0x80) && !(sv->cFlags24 & 0x40000000)) {
            sv->cFlags24 &= ~0x90;
            sound_hw_release_hold(i);
        }
        if (sv->age28 != 0) {
            sv->age28 -= sv->ageSpeed2C * sound_tick_step;
            if ((s32)sv->age28 < 0) {
                sv->age28 = 0;
            }
        }
        if (sv->cFlags24 & 0x10000) {
            sv->panning38 = sv->panTargetAC - (sv->panTimeA8 >> 8) * sv->panDelta3C;
            sv->panning38 = sv->panning38 < 0 ? 0 : (u32)sv->panning38 > 0x7F0000U ? 0x7F0000U : sv->panning38;
            sv->panTimeA8 -= sound_tick_step;
            if (sv->panTimeA8 == 0) {
                sv->cFlags24 &= ~0x10000;
                sv->panning38 = sv->panTargetAC;
            }
        }
        ccents = (sv->curNote50 << 16) + (sv->curDetuneC0 << 16) / 100;
        if (sv->cFlags24 & 0x40010) {
            if (sv->midi4A != 0xFF) {
                pbend = sound_synth_get_pitch_bend_input(sv);
                sv->pbLastB8 = pbend;
                goto pbend_adjust;
            }
        } else {
            pbend = sv->pbLastB8;
        pbend_adjust:
            if (pbend != 0x2000) {
                pbend -= 0x2000;
                if (pbend < 0) {
                    ccents += sv->pbLowerKeyRange6A * pbend * 8;
                } else {
                    ccents += sv->pbUpperKeyRange6B * pbend * 8;
                }
            }
        }
        if (sv->cFlags24 & 0x4000) {
            vrange = (sv->vibKeyRange78 << 8) + (sv->vibCentRange79 << 8) / 100;
            if ((sv->cFlags24 & 0x8000) && sv->midi4A != 0xFF) {
                voff = ((sound_synth_get_modulation_input(sv) >> 7) * sv->vibCurOffset74) >> 7;
            } else {
                voff = sv->vibCurOffset74;
            }
            ccents += (vrange * voff) >> 4;
        }
        cpitch = sv->sweepOff40[1] + (sv->sweepOff40[0] + sound_synth_key_to_pitch((struct VoicePrefix *)sv, sound_synth_apply_portamento((struct RampState *)sv, ccents)));
        if (sv->midi4A != 0xFF) {
            cpitch = (sound_synth_get_doppler_input(sv) * (cpitch >> 16)) >> 13;
        } else {
            cpitch >>= 16;
        }
        span = cpitch;
        sound_hw_set_pitch(i, span);
        span = 0;
        if (sv->cFlags24 & 0x20000) {
            sv->volume30 = sv->envTarget88 - (sv->envTimeB4 >> 8) * sv->envDelta84;
            sv->volume30 = sv->volume30 < 0 ? 0 : (u32)sv->volume30 > 0x7F0000U ? 0x7F0000U : sv->volume30;
            sv->envTimeB4 -= sound_tick_step;
            if ((s32)sv->envTimeB4 <= 0) {
                sv->cFlags24 &= ~0x20000;
                sv->volume30 = sv->envTarget88;
            }
        }
        vol = sv->volume30;
        pan = sv->panning38;
        midi = sv->midi4A;
        vol = (VOLUME_FADERS[sv->vGroupBE].pauseVolume >> 16) * (vol >> 7);
        vol = (VOLUME_FADERS[sv->vGroupBE].volume >> 16) * (vol >> 7);
        vol = (VOLUME_FADERS[sv->fxFlag4C ? 22 : 21].volume >> 16) * (vol >> 7);
        if (sv->track2F != 0xFF) {
            vol = sound_track_volumes[sv->track2F] * (vol >> 7);
        }
        if (sound_mono == 0) {
            if ((span = midi) != 0xFF) {
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
            if (midi != 0xFF) {
                vol = (sound_synth_get_volume_input(sv) >> 7) * (vol >> 7);
            }
            pan = 0x400000;
        }
        if (midi != 0xFF) {
            aux = ((sound_synth_get_reverb_input(sv) >> 7) * vol) >> 7;
            if (aux > 0x7F0000) {
                aux = 0x7F0000;
            }
            aux = (aux >> 7) * sv->revVolScale99;
            if (aux > 0x7F0000) {
                aux = 0x7F0000;
            }
            aux = (sv->revVolOffset9A << 16) + aux;
            if (aux > 0x7F0000) {
                aux = 0x7F0000;
            }
        }
        else {
            aux = 0;
        }
        sv->curOutputVolumeC2 = vol >> 8;
        sound_hw_set_volume(i, vol, pan, span, aux);
    }
    for (j = 0; j < 32; j++) {
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
                case 1:
                    sound_seq_stop_unlocked(VOLUME_FADERS[j].seqId);
                    break;
                case 2:
                    sound_seq_pause_unlocked(VOLUME_FADERS[j].seqId);
                    break;
                case 3:
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

/* PROMOTED 2026-10-04 — sound_synth_tick
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern s32 sound_samples_per_tick;
extern s32 sound_synth_frequency;
extern u32 sound_tick_step;
extern void sound_synth_update_voices(void);
void sound_synth_tick(void)
{
    if (sound_samples_per_tick != 0) {
        sound_tick_step = (u32)((s32)((u32)sound_samples_per_tick * 32000U) / sound_synth_frequency) << 3;
        sound_synth_update_voices();
    }
}

/* PROMOTED 2026-10-11 — sound_start_sfx_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct Preset { u16 identifier; u16 high; u8 low; u8 middle; u8 default_first; u8 default_second; u8 flags; u8 parameter; u8 unknown0A[2]; } Preset;
#pragma pack(0)
extern Preset *sound_find_sfx(u16);
extern u32 sound_synth_start_note(u32, u8, u8, u8, u8, u8, u16, u16, u8, s16);
u32 sound_start_sfx_unlocked(u16 identifier, u8 first, u8 second)
{
    Preset *preset;
    u32 result;
    result = 0xFFFFFFFFU;
    preset = sound_find_sfx(identifier);
    if (preset != 0) {
        if (first == 255) first = preset->default_first;
        if (second == 255) second = preset->default_second;
        result = sound_synth_start_note(((u32)preset->high << 16) |
            ((u32)preset->middle << 8) | preset->low,
            preset->flags | 128, first, second, 255, 255, 0, 255,
            preset->parameter, 0);
    }
    return result;
}

/* PROMOTED 2026-10-05 — sound_set_sfx_pan_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern int sound_resolve_handle(u32);
extern void sound_midi_set_controller(u8, u8, u8, u8);
int sound_set_sfx_pan_unlocked(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != 0xFFFFFFFFU) {
        index = identifier & 255;
        if (sound_synth_voices[index].identifier == identifier) {
            result = 0;
            if (sound_synth_voices[index].flags & 2) {
                sound_midi_set_controller(10, index, sound_synth_voices[index].channel55, value);
            } else {
                sound_midi_set_controller(10, index, sound_synth_voices[index].set, value);
            }
            identifier = sound_synth_voices[index].child;
        } else {
            return result;
        }
    }
    return result;
}

/* PROMOTED 2026-10-05 — sound_set_sfx_span_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
int sound_set_sfx_span_unlocked(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != 0xFFFFFFFFU) {
        index = identifier & 255;
        if (sound_synth_voices[index].identifier == identifier) {
            result = 0;
            if (sound_synth_voices[index].flags & 2) {
                sound_midi_set_controller(131, index, sound_synth_voices[index].channel55, value);
            } else {
                sound_midi_set_controller(131, index, sound_synth_voices[index].set, value);
            }
            identifier = sound_synth_voices[index].child;
        } else {
            return result;
        }
    }
    return result;
}

/* PROMOTED 2026-10-11 — sound_set_sfx_doppler_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
int sound_set_sfx_doppler_unlocked(u32 identifier, u16 value)
{
    int result;
    u32 index;
    u8 low;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != 0xFFFFFFFFU) {
        index = identifier & 255;
        if (sound_synth_voices[index].identifier == identifier) {
            low = value & 127;
            if (sound_synth_voices[index].flags & 2) {
                sound_midi_set_controller(132, index, sound_synth_voices[index].channel55, value >> 7);
                sound_midi_set_controller(133, index, sound_synth_voices[index].channel55, low);
            } else {
                sound_midi_set_controller(132, index, sound_synth_voices[index].set, value >> 7);
                sound_midi_set_controller(133, index, sound_synth_voices[index].set, low);
            }
            result = 0;
            identifier = sound_synth_voices[index].child;
        } else {
            return result;
        }
    }
    return result;
}

/* PROMOTED 2026-10-11 — sound_set_sfx_pitch_bend_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
int sound_set_sfx_pitch_bend_unlocked(u32 identifier, u16 value)
{
    int result;
    u32 index;
    u8 low;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != 0xFFFFFFFFU) {
        index = identifier & 255;
        if (sound_synth_voices[index].identifier == identifier) {
            low = value & 127;
            if (sound_synth_voices[index].flags & 2) {
                sound_midi_set_controller(128, index, sound_synth_voices[index].channel55, value >> 7);
                sound_midi_set_controller(129, index, sound_synth_voices[index].channel55, low);
            } else {
                sound_midi_set_controller(128, index, sound_synth_voices[index].set, value >> 7);
                sound_midi_set_controller(129, index, sound_synth_voices[index].set, low);
            }
            result = 0;
            identifier = sound_synth_voices[index].child;
        } else {
            return result;
        }
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_synth_copy_controllers
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
struct VoiceState;
extern void sound_midi_copy_voice_controller(u8, struct VoiceState *, struct VoiceState *);
void sound_synth_copy_controllers(struct VoiceState *destination, struct VoiceState *source)
{
    sound_midi_copy_voice_controller(7, destination, source);
    sound_midi_copy_voice_controller(10, destination, source);
    sound_midi_copy_voice_controller(91, destination, source);
    sound_midi_copy_voice_controller(128, destination, source);
    sound_midi_copy_voice_controller(132, destination, source);
}

/* PROMOTED 2026-10-05 — sound_set_sfx_volume_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
int sound_set_sfx_volume_unlocked(u32 identifier, u8 value)
{
    int result;
    u32 index;
    result = -1;
    identifier = sound_resolve_handle(identifier);
    while (identifier != 0xFFFFFFFFU) {
        index = identifier & 255;
        if (sound_synth_voices[index].identifier == identifier) {
            result = 0;
            if (sound_synth_voices[index].flags & 2) {
                sound_midi_set_controller(7, index, sound_synth_voices[index].channel55, value);
            } else {
                sound_midi_set_controller(7, index, sound_synth_voices[index].set, value);
            }
            identifier = sound_synth_voices[index].child;
        } else {
            return result;
        }
    }
    return result;
}

/* PROMOTED 2026-10-05 — sound_key_off_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern u8 sound_initialized;
int sound_key_off_unlocked(u32 key)
{
    u32 index;
    int result;
    result = -1;
    if (sound_initialized) {
        key = sound_resolve_handle(key);
        while (key != 0xFFFFFFFFU) {
            index = key & 255;
            if (sound_synth_voices[index].identifier == key) {
                sound_synth_voices[index].flags |= 8;
                result = 0;
            }
            key = sound_synth_voices[index].child;
        }
    }
    return result;
}

/* PROMOTED 2026-10-05 — sound_get_output_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
u16 sound_get_output_volume(u32 key)
{
    int identifier;
    identifier = sound_resolve_handle(key);
    if (identifier != -1) {
        if (sound_synth_voices[identifier & 255].identifier == (u32)identifier &&
            !(sound_synth_voices[identifier & 255].flags & 2)) {
            return sound_synth_voices[identifier & 255].valueC2;
        }
    }
    return 0;
}

/* PROMOTED 2026-10-11 — sound_fade_volume_group_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct MasterFader { u32 volume; u32 target; s32 delta; u32 time; u32 seqId; u8 type; u8 seqMode; u8 unknown16[2]; u32 pauseVolume; u32 pauseTarget; s32 pauseDelta; u32 pauseTime; } MasterFader;
extern void sound_ms_to_time(u32 *);
void sound_fade_volume_group_unlocked(u8 volume, u16 time, u8 vGroup, u8 seqMode, u32 seqId)
{
    u32 ltime;
    u32 i;
    u8 type;

    if (time == 0) {
        ++time;
    }
    ltime = time;
    sound_ms_to_time(&ltime);
    switch (vGroup) {
    case 255:
        for (i = 0; i < 32; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 0 || ((MasterFader *)sound_volume_faders)[i].type == 1) {
                ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].time = ltime;
                ((MasterFader *)sound_volume_faders)[i].delta = (s32)(((MasterFader *)sound_volume_faders)[i].target - ((MasterFader *)sound_volume_faders)[i].volume) / time;
                ((MasterFader *)sound_volume_faders)[i].seqId = 0xFFFFFFFFU;
            }
        }
        return;
    case 252:
        for (i = 0; i < 32; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 2 || ((MasterFader *)sound_volume_faders)[i].type == 3) {
                ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].time = ltime;
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
        for (i = 0; i < 32; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == type) {
                ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].time = ltime;
                ((MasterFader *)sound_volume_faders)[i].delta = (s32)(((MasterFader *)sound_volume_faders)[i].target - ((MasterFader *)sound_volume_faders)[i].volume) / time;
                ((MasterFader *)sound_volume_faders)[i].seqId = 0xFFFFFFFFU;
            }
        }
        return;
    default:
        i = vGroup;
        ((MasterFader *)sound_volume_faders)[i].target = volume << 16;
        ((MasterFader *)sound_volume_faders)[i].time = ltime;
        ((MasterFader *)sound_volume_faders)[i].delta = (s32)(((MasterFader *)sound_volume_faders)[i].target - ((MasterFader *)sound_volume_faders)[i].volume) / time;
        ((MasterFader *)sound_volume_faders)[i].seqMode = seqMode;
        ((MasterFader *)sound_volume_faders)[i].seqId = seqId;
        return;
    }
}

/* PROMOTED 2026-10-11 — sound_is_group_fading_out
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_is_group_fading_out(u8 index)
{
    ChannelState *state;
    state = &sound_volume_faders[index];
    if (state->kind14 != 4 && state->pending0C != 0 && (int)state->flags08 < 0) return 1;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_fade_pause_volume_unlocked
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_fade_pause_volume_unlocked(u8 volume, u16 time, u8 group)
{
    u32 i;
    u32 ltime;
    u8 type;

    if (time == 0) {
        ++time;
    }
    ltime = time;
    sound_ms_to_time(&ltime);
    switch (group) {
    case 255:
        for (i = 0; i < 32; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 0 || ((MasterFader *)sound_volume_faders)[i].type == 1) {
                ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].pauseTime = ltime;
                ((MasterFader *)sound_volume_faders)[i].pauseDelta = (s32)(((MasterFader *)sound_volume_faders)[i].pauseTarget - ((MasterFader *)sound_volume_faders)[i].pauseVolume) / time;
            }
        }
        return;
    case 252:
        for (i = 0; i < 32; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == 2 || ((MasterFader *)sound_volume_faders)[i].type == 3) {
                ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].pauseTime = ltime;
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
        for (i = 0; i < 32; ++i) {
            if (((MasterFader *)sound_volume_faders)[i].type == type) {
                ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
                ((MasterFader *)sound_volume_faders)[i].pauseTime = ltime;
                ((MasterFader *)sound_volume_faders)[i].pauseDelta = (s32)(((MasterFader *)sound_volume_faders)[i].pauseTarget - ((MasterFader *)sound_volume_faders)[i].pauseVolume) / time;
            }
        }
        return;
    default:
        i = group;
        ((MasterFader *)sound_volume_faders)[i].pauseTarget = volume << 16;
        ((MasterFader *)sound_volume_faders)[i].pauseTime = ltime;
        ((MasterFader *)sound_volume_faders)[i].pauseDelta = (s32)(((MasterFader *)sound_volume_faders)[i].pauseTarget - ((MasterFader *)sound_volume_faders)[i].pauseVolume) / time;
        return;
    }
}

/* PROMOTED 2026-10-11 — sound_set_group_type
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
extern unsigned char sound_volume_fader_types[][40];
void sound_set_group_type(unsigned char channel, unsigned char value)
{
    if (sound_initialized) {
        sound_volume_fader_types[channel][0] = value;
    }
}

/* PROMOTED 2026-10-05 — sound_synth_init
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
#pragma pack(0)
extern short sound_global_variables[16];
extern u32 sound_synth_unread_setting;
extern u8 sound_mono;
extern void sound_synth_set_tempo(u32,u8);
extern void sound_init_handles(void);
extern void sound_init_voice_allocation(void);
extern void sound_synth_init_channel_owners(void);
void sound_synth_init(u32 configuration)
{
    int i;
    sound_synth_frequency = configuration;
    sound_synth_unread_setting = 10240;
    sound_synth_set_tempo(120,255);
    sound_mono = 0;
    for (i=0;i<PC_MX_LOOP(32);i++) {
        sound_synth_voices[i].identifier = 0xFFFFFFFFU;
        sound_synth_voices[i].command00 = 0;
        sound_synth_voices[i].flags = 0;
        sound_synth_voices[i].age28 = 0;
        sound_synth_voices[i].channel2E = 0;
        sound_synth_voices[i].loop68 = 0;
        sound_synth_voices[i].channel = 255;
        sound_synth_voices[i].volume30 = 0;
        sound_synth_voices[i].depth99 = 128;
        sound_synth_voices[i].flag9A = 0;
        sound_synth_voices[i].panning38 = 0x3F0000;
        sound_synth_voices[i].words40[0] = 0;
        sound_synth_voices[i].words40[1] = 0;
        sound_synth_voices[i].status48 = 0;
        sound_synth_voices[i].status49 = 0;
        sound_synth_voices[i].activeBD = 0;
        sound_synth_voices[i].groupBE = 23;
        sound_synth_voices[i].word16C = 0;
        sound_synth_voices[i].half170 = 0;
        sound_synth_voices[i].word178 = 0;
        sound_synth_voices[i].half17C = 0;
        sound_synth_voices[i].glide = 100;
        sound_synth_voices[i].state98 = 0;
    }
    for (i=0;i<32;i++) {
        sound_volume_faders[i].word00 = 0;
        sound_volume_faders[i].word04 = 0;
        sound_volume_faders[i].pending0C = 0;
        sound_volume_faders[i].kind14 = 4;
        sound_volume_faders[i].value18 = 0x7F0000;
    }
    sound_volume_faders[31].kind14 = 1;
    for (i=0;i<8;i++) sound_volume_faders[i+23].kind14 = 0;
    sound_volume_faders[21].word00 = 0x7F0000;
    sound_volume_faders[22].word00 = 0x7F0000;
    sound_init_handles();
    sound_init_voice_allocation();
    for (i=0;i<16;i++) {
        sound_global_variables[i] = 0;
    }
    sound_synth_init_channel_owners();
}

