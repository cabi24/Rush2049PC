/* src/rom/sound/streams_3d.c: the MusyX sound library's streams and its 3D sound (emitters and listeners).
 *
 * A stream is a voice that plays a ring of PCM16 samples the CPU fills: sound_stream_start takes a voice
 * (sound_alloc_stream_voice) and a ring, and each frame sound_stream_refill (from sound_hw_service_frame) tells
 * the owner's callback how far the voice has played, so it can refill behind it. Only the speech module
 * (src/rom/speech) uses streams, and the game never starts it.
 *
 * 3D sound: listeners (position, velocity, orientation, surround distances, speed of sound) and emitters (position,
 * velocity, range, volume curve, an effect). Each frame sound_emitter_update_all computes every emitter's volume,
 * pan, surround and doppler from the listeners (sound_emitter_compute), starts, updates, pauses or removes its
 * voice, and limits each group of emitters to the voices it has, starting the loudest candidates
 * (sound_emitter_note_*, sound_emitter_start_candidates). The game keeps its own emitters and listeners
 * (audiomgr: AudEmitEffect, AudUpdateListener ...) and never adds any here, so the list is always empty.
 *
 * MusyX library code with no MAT3 counterpart. The emitter and listener records are the caller's storage, laid out
 * natively on the PC (host-sized links); two functions still read them at N64 offsets (marked "PC:" below). */
#include "../rom_tu.h"
#include "sound_sample_record.h"

#define SOUND_MIDI_MAX 127          /* full volume, the largest MIDI value */
#define SOUND_PAN_CENTER 64
#define SOUND_PITCH_MAX 16383       /* the largest doppler/pitch factor (8192 is 1.0) */
#define SOUND_EMITTER_GROUP_MAX 32  /* groups, candidates and playing emitters noted per frame */
#define SOUND_EMITTER_STEAL_FRAMES 20 /* frames a close candidate waits before it starts anyway */

/* An emitter's flags (StateNode.flags08). The low 16 bits are the caller's (sound_emitter_add), the rest the
 * library's own state. */
#define SOUND_EMITTER_CONTINUOUS 0x1        /* updated every frame, and voice-limited within its group */
#define SOUND_EMITTER_RESTARTABLE 0x2       /* stays when its voice ends or cannot start, and starts again */
#define SOUND_EMITTER_PAUSABLE 0x4          /* out of range (volume 0): its voice stops, and starts again in range */
#define SOUND_EMITTER_DOPPLER 0x8           /* doppler even if the listener has none */
#define SOUND_EMITTER_USER_FLAGS 0xFFFF
#define SOUND_EMITTER_ACTIVE 0x10000        /* in sound_emitters */
#define SOUND_EMITTER_START 0x20000         /* its voice is to start */
#define SOUND_EMITTER_REMOVE 0x40000        /* to be unlinked at the next update */
#define SOUND_EMITTER_PAUSED 0x80000        /* silent and voiceless until back in range */
#define SOUND_EMITTER_FADE_IN 0x100000      /* volume scaled by `fade`, rising each frame */
/* A listener's flags */
#define SOUND_LISTENER_DOPPLER 0x1          /* every emitter gets doppler for this listener */

/* A stream's state (SampleBuffer.mode) */
#define SOUND_STREAM_IDLE 0
#define SOUND_STREAM_PLAYING 1

/* On the PC a voice's stream record is PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9): a stream plays on the voice sound_alloc_stream_voice gives it, so its record is that voice's. */
#define sound_streams (*(SampleBuffer (*)[PC_MX_VOICE_SLOTS])pc_mx_streams)

#pragma pack(1)
typedef struct SoundHwSampleInfo { unsigned int frequency; void *data; unsigned int offset; unsigned int length; unsigned int loopStart; unsigned int loopLength; unsigned char format; } SoundHwSampleInfo;
#pragma pack()

/* The emitter record, as sound_emitter_compute reads it (the same record as StateNode, sound_sample_record.h, and
 * SoundEmitterAddView below: next, previous, flags, then these). */
typedef struct SoundVector { float x, y, z; } SoundVector;
typedef struct SoundEmitter { StateNode *next, *previous; u32 flags; SoundVector pos, dir; float maxDis, maxVol, minVol, volPush; } SoundEmitter;
/* The listener record (SpatialState and the listener views below are the same record). heading, right and up are
 * its axes, mat the inverse of its transform (world to listener space). */
typedef struct SoundListener { struct SoundListener *next, *previous; u32 flags; SoundVector pos, dir, heading, right, up; float mat[12]; float surroundDisFront, surroundDisBack, soundSpeed, vol; } SoundListener;
#define EM ((SoundEmitter *)em)

typedef struct Vector { float x, y, z; } Vector;
/* PC: this view keeps the N64 layout (position at 0xC, where the PC's record has it at 0x14, after two host-sized
 * links): see sound_emitter_update. */
typedef struct SoundEmitterUpdateView { u8 unknown00[12]; Vector position; Vector velocity; unsigned int unknown24; float gain28; float current2C; } SoundEmitterUpdateView;
/* The emitter as sound_emitter_add fills it: range, gain (the volume at the emitter), minimum (the volume at the
 * edge of range) and curve (0 linear, 1 square law) shape the volume by distance; handle is its voice, context its
 * group, identifier its effect, counter how long it has waited to start (sound_emitter_start_candidates). */
typedef struct SoundEmitterAddView { StateNode *next, *previous; u32 flags; Vector position, velocity; float range, gain, minimum, curve; u32 handle, context; u16 identifier, counter; float fade; } SoundEmitterAddView;
/* PC: a listener (SoundListener, sound_listener_add's view) has two host-sized links before its flags, so its vectors
 * start at +0x14, not +0xC. */
typedef struct SpatialState { LinkNode *next, *previous; u32 flags08; Vector position; unsigned char unknown18[12]; Vector forward; Vector side; Vector up; float inverse[12]; } SpatialState;
/* PC: the same listener, as sound_listener_add links it (SpatialState above). */
typedef struct SoundListenerUpdateView { LinkNode *next, *previous; u32 flags08; Vector position; Vector velocity; Vector forward; Vector side; Vector up; float inverse[12]; u8 unknown78[12]; float gain84; } SoundListenerUpdateView;
typedef struct SoundListenerAddView { LinkNode *next, *previous; u32 flags08; Vector position, velocity, forward, side, up; float inverse[12]; float surround_front, surround_back, sound_speed, gain84; } SoundListenerAddView;

/* The per-frame voice limit's lists: per group of emitters, the candidates to start (the large nodes) and the
 * emitters playing (the small nodes), with each one's computed volume, pans and doppler. */
typedef struct SmallNode { struct SmallNode *next; float volume; StateNode *object; } SmallNode;
typedef struct LargeNode { struct LargeNode *next; float volume, x_pan, y_pan, z_pan, doppler; StateNode *object; } LargeNode;
typedef struct EmitGroup { unsigned int identifier; LargeNode *large; SmallNode *small; } EmitGroup;

/* The library's data */
extern unsigned char sound_voice_count;
extern unsigned char sound_initialized;
extern int sound_synth_frequency;
extern LinkNode *sound_listeners;
extern StateNode *sound_emitters;
extern SoundEmitterAddView sound_oneshot_emitter;
extern unsigned char sound_emitter_group_count, sound_emitter_candidate_count, sound_emitter_playing_count;
extern EmitGroup sound_emitter_groups[SOUND_EMITTER_GROUP_MAX];
extern LargeNode sound_emitter_candidates[SOUND_EMITTER_GROUP_MAX];
extern SmallNode sound_emitter_playing[SOUND_EMITTER_GROUP_MAX];
extern float sound_emitter_steal_margin, sound_emitter_keep_margin;
extern const float sound_emitter_fade_step;

/* The library's functions */
extern u32 sound_hw_get_voice_position(int);
extern void sound_hw_flush_stream(short *, u32);
extern void sound_hw_flush_stream_wrap(short *, unsigned int);
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern int sound_alloc_stream_voice(unsigned char);
extern void sound_hw_set_voice_sample(unsigned int, SoundHwSampleInfo *, unsigned char);
extern void sound_hw_set_pitch(int, unsigned short);
extern void sound_hw_set_volume(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
extern void sound_hw_start_voice(unsigned int);
extern void sound_free_stream_voice(unsigned int);
extern float sqrtf(float);
extern void sound_transform_point(const void *, const void *, void *);
extern float sound_normalize(void *);
extern u8 sound_clamp_midi_value(u8);
extern u16 sound_clamp_pitch_value(u32);
extern int sound_set_sfx_volume_unlocked(u32, u8);
extern int sound_set_sfx_pan_unlocked(u32, u8);
extern int sound_set_sfx_span_unlocked(u32, u8);
extern int sound_set_sfx_doppler_unlocked(u32, u16);
extern int sound_key_off_unlocked(u32);
extern void sound_emitter_compute(StateNode *, float *, float *, float *, float *, float *);
extern void sound_emitter_apply(StateNode *em, float vol, float xPan, float yPan, float zPan, float doppler);
extern u32 sound_start_sfx(u16, u8, u8);
extern int sound_emitter_add(void *, const void *, const void *, float, float, u32, u16, u32, u8, u8);
extern void sound_emitter_unlink(StateNode *);
extern int sound_emitter_remove(StateNode *);
extern void sound_cross(Vector *, const Vector *, const Vector *);
extern void sound_invert_transform(float *, const float *);
extern void sound_listener_build_matrix(SpatialState *);
extern unsigned int sound_start_sfx_unlocked(unsigned short, unsigned char, unsigned char);
extern void sound_emitter_reset_groups(void);
extern int sound_emitter_note_candidate(StateNode *, float, float, float, float, float);
extern unsigned int sound_check(unsigned int);
extern void sound_emitter_note_playing(StateNode *, float);
extern void sound_emitter_start_candidates(void);

/* Marks every voice's stream record idle (the library's start-up). */
void sound_stream_reset_all(void)
{
    int i;
    for (i = 0; i < sound_voice_count; i++) {
        sound_streams[i].mode = SOUND_STREAM_IDLE;
    }
}

/* Once a frame (sound_hw_service_frame): for each playing stream whose voice has moved on, calls its owner's
 * callback with the part of the ring played since last time (in two pieces when it wraps), so the owner can
 * write new samples there; if the callback says it wrote, writes those samples back from the CPU's data cache
 * (sound_hw_flush_stream; the RSP reads memory, not the cache). */
void sound_stream_refill(void)
{
    int i;
    u32 position;
    for (i = 0; i < sound_voice_count; i++) {
        if (sound_streams[i].mode == SOUND_STREAM_PLAYING) {
            position = sound_hw_get_voice_position(i);
            if (position != sound_streams[i].position) {
                if (position > sound_streams[i].position) {
                    if (sound_streams[i].callback(sound_streams[i].buffer + sound_streams[i].position,
                        position - sound_streams[i].position, 0, 0, sound_streams[i].context)) {
                        sound_hw_flush_stream(sound_streams[i].buffer + sound_streams[i].position,
                            position - sound_streams[i].position);
                    }
                } else {
                    if (sound_streams[i].callback(sound_streams[i].buffer + sound_streams[i].position,
                        sound_streams[i].samples - sound_streams[i].position,
                        sound_streams[i].buffer, position, sound_streams[i].context)) {
                        sound_hw_flush_stream(sound_streams[i].buffer + sound_streams[i].position,
                            sound_streams[i].samples - sound_streams[i].position);
                        sound_hw_flush_stream(sound_streams[i].buffer, position);
                    }
                }
            }
            sound_streams[i].position = position;
        }
    }
}

/* Once a frame: copies the first four samples of each playing stream's ring past its end (the 8 spare bytes,
 * sound_hw_flush_stream_wrap), so a voice that reads across the wrap reads on. */
void sound_stream_flush_wraps(void)
{
    int i;
    for (i = 0; i < sound_voice_count; i++) {
        if (sound_streams[i].mode == SOUND_STREAM_PLAYING) {
            sound_hw_flush_stream_wrap(sound_streams[i].buffer, sound_streams[i].samples);
        }
    }
}

/* Starts a stream: a voice of the given priority plays the ring `buffer` of `samples` PCM16 samples at
 * `frequency` Hz, looping, with volume, pan, span and aux send (0..127). callback(start, count, start2, count2,
 * context) is told each frame what the voice has played (sound_stream_refill). Returns the voice, which is the
 * stream's handle, or -1. */
int sound_stream_start(unsigned char priority, short *buffer, unsigned int samples, unsigned int frequency,
                  unsigned char volume, unsigned char pan, unsigned char span, unsigned char aux,
                  SampleCallback callback, unsigned int context)
{
    int index;
    SoundHwSampleInfo info;
    float ratio;
    if (sound_initialized) {
        sound_hw_lock();
        index = sound_alloc_stream_voice(priority);
        if (index != -1) {
            sound_streams[index].callback = callback;
            sound_streams[index].mode = SOUND_STREAM_PLAYING;
            sound_streams[index].context = context;
            sound_streams[index].position = 0;
            info.frequency = frequency | 0x400000;
            info.offset = 0;
            info.loopStart = 0;
            info.format = 0;
            sound_streams[index].samples = samples;
            info.length = samples;
            info.loopLength = samples;
            sound_streams[index].buffer = buffer;
            info.data = buffer;
            sound_hw_set_voice_sample(index, &info, 1);
            ratio = (float)frequency / (float)sound_synth_frequency;
            sound_hw_set_pitch(index, (unsigned int)(ratio * 4096.0f));
            sound_hw_set_volume(index, volume << 16, pan << 16, span << 16, aux << 16);
            sound_hw_start_voice(index);
        }
        sound_hw_unlock();
        return index;
    }
    return -1;
}

/* The bytes a stream's ring of `count` samples needs: two per sample and 8 more for the wrap copy. */
unsigned int sound_stream_buffer_size(unsigned int count)
{
    return count * 2 + 8;
}

/* Sets a playing stream's volume, pan, span and aux send (0..127). voice: sound_stream_start's handle. */
void sound_stream_set_volume(unsigned int voice, unsigned char volume, unsigned char pan, unsigned char span, unsigned char aux)
{
    if (sound_initialized && voice != 0xFFFFFFFF) {
        sound_hw_lock();
        sound_hw_set_volume(voice, volume << 16, pan << 16, span << 16, aux << 16);
        sound_hw_unlock();
    }
}

/* Stops a stream and frees its voice. voice: sound_stream_start's handle. */
void sound_stream_stop(unsigned int voice)
{
    if (sound_initialized) {
        sound_hw_lock();
        if (sound_streams[voice].mode == SOUND_STREAM_PLAYING) {
            sound_streams[voice].mode = SOUND_STREAM_IDLE;
            sound_free_stream_voice(voice);
        }
        sound_hw_unlock();
    }
}

/* Computes what the listeners hear of emitter em: *vol, the sum over the listeners in range of the listener's
 * volume times the emitter's volume at that distance (from maxVol at the emitter to minVol at maxDis, the curve
 * between linear and square law by volPush); *doppler, the pitch factor from the relative speed (closing in or
 * moving apart a frame ahead), when the emitter or the listener asks for doppler; and the pans averaged over all
 * listeners: *xPan (left/right) and *yPan (up/down) from the direction in the listener's space, *zPan (front/back)
 * from the distance in front or behind relative to the listener's surround distances. A paused emitter gets only
 * its volume. With no listeners the pans are left as they were. */
void sound_emitter_compute(StateNode *em, float *vol, float *doppler, float *xPan, float *yPan, float *zPan)
{
    SoundListener *li;
    SoundVector d;
    SoundVector v;
    SoundVector p;
    float relspeed;
    float distance;
    float new_distance;
    float ft;
    float vd;
    float panz;
    float pany;
    float panx;
    u32 n;

    ft = 1.f / 60.f;
    *vol = 0.f;
    *doppler = 1.f;
    panx = pany = panz = 0.f;
    for (n = 0, li = (SoundListener *)sound_listeners; li != NULL; li = li->next, ++n) {
        if (n) {}
        d.x = EM->pos.x - li->pos.x;
        d.y = EM->pos.y - li->pos.y;
        d.z = EM->pos.z - li->pos.z;
        distance = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        if (EM->maxDis >= distance) {
            vd = distance / EM->maxDis;
            *vol += li->vol * (EM->minVol + (EM->maxVol - EM->minVol) *
                                (1.f - ((1.f - EM->volPush) * vd + EM->volPush * vd * vd)));
            if (!(EM->flags & SOUND_EMITTER_PAUSED)) {
                if ((EM->flags & SOUND_EMITTER_DOPPLER) || (li->flags & SOUND_LISTENER_DOPPLER)) {
                    v.x = li->dir.x - EM->dir.x;
                    v.y = li->dir.y - EM->dir.y;
                    v.z = li->dir.z - EM->dir.z;
                    relspeed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
                    if (relspeed > 0.f) {
                        d.x = (EM->pos.x + EM->dir.x * ft) - (li->pos.x + li->dir.x * ft);
                        d.y = (EM->pos.y + EM->dir.y * ft) - (li->pos.y + li->dir.y * ft);
                        d.z = (EM->pos.z + EM->dir.z * ft) - (li->pos.z + li->dir.z * ft);
                        new_distance = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
                        if (new_distance < distance) {
                            *doppler = li->soundSpeed / (li->soundSpeed - relspeed);
                        } else {
                            *doppler = li->soundSpeed / (li->soundSpeed + relspeed);
                        }
                    }
                }
                if (distance != 0.f) {
                    sound_transform_point(&li->mat, &EM->pos, &p);
                    if (p.z >= 0.f) {
                        panz += p.z < li->surroundDisFront ? p.z / li->surroundDisFront : 1.0;
                    } else {
                        panz += -li->surroundDisBack < p.z ? p.z / li->surroundDisBack : -1.0;
                    }
                    if (p.x != 0.0 || p.y != 0.0 || p.z != 0) {
                        sound_normalize(&p);
                    }
                    panx += p.x;
                    pany += p.y;
                }
            }
        }
    }
    if (n != 0) {
        *xPan = panx / n;
        *yPan = pany / n;
        *zPan = panz / n;
    }
}

/* Clamps a value to MIDI's 0..127. */
unsigned char sound_clamp_midi_value(unsigned char value)
{ if (value > SOUND_MIDI_MAX) return SOUND_MIDI_MAX; return value; }

/* Clamps a pitch factor to 14 bits (8192 is 1.0). */
unsigned short sound_clamp_pitch_value(unsigned int value)
{
    if (value > SOUND_PITCH_MAX) {
        return SOUND_PITCH_MAX;
    }
    return value;
}

/* PC: IDO's float to u32 (pc_prelude.h), here also the implicit one of the doppler factor, which is negative once
 * a source closes in faster than sound: 0xFFFFFFFF, the top pitch, on the N64 and x86-64, 0 on AArch64. */
#define F2U_ARG(x) pc_mips_f2u(x)
/* Applies computed volume (0..1, scaled by the fade while fading in), pans (-1..1) and doppler factor to the
 * emitter's voice, as MIDI-range volume, pan and span and a 14-bit pitch factor. zPan's front is span 0. */
void sound_emitter_apply(StateNode *em, float vol, float xPan, float yPan,
                   float zPan, float doppler)
{
    u32 identifier;
    identifier = em->identifier34;
    if (em->flags08 & SOUND_EMITTER_FADE_IN) {
        sound_set_sfx_volume_unlocked(identifier, sound_clamp_midi_value(N64_F2U((em->fade * vol) * 127.0f)));
    } else {
        sound_set_sfx_volume_unlocked(identifier, sound_clamp_midi_value(N64_F2U(vol * 127.0f)));
    }
    sound_set_sfx_pan_unlocked(identifier, sound_clamp_midi_value(N64_F2U((1.0f + xPan) * 64.0f)));
    sound_set_sfx_span_unlocked(identifier, sound_clamp_midi_value(N64_F2U((1.0f - zPan) * 64.0f)));
    sound_set_sfx_doppler_unlocked(identifier, sound_clamp_pitch_value(F2U_ARG(doppler * 8192.0f)));
}

/* Takes an emitter out of sound_emitters, drops the library's flags and keys its voice off. */
void sound_emitter_unlink(StateNode *state)
{
    if (state->next != 0) state->next->previous = state->previous;
    if (state->previous != 0) state->previous->next = state->next;
    else sound_emitters = state->next;
    state->flags08 &= SOUND_EMITTER_USER_FLAGS;
    if (state->identifier34 != 0xFFFFFFFF) sound_key_off_unlocked(state->identifier34);
}

/* Moves an emitter: new position and velocity, and its volume (level, 0..127); a lower volume also lowers its
 * minimum. Returns 1, or 0 if the library is not started.
 * PC: the view has the N64 layout (see its type), so on the PC this would write the vectors over the links and
 * flags. Nothing calls it: the game keeps its own emitters. */
int sound_emitter_update(SoundEmitterUpdateView *state, const Vector *position, const Vector *velocity, u8 level)
{
    if (sound_initialized) {
        sound_hw_lock();
        state->position = *position;
        state->velocity = *velocity;
        state->gain28 = (float)level / 127.0f;
        if (state->gain28 < state->current2C) state->current2C = state->gain28;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* Is the emitter in the list (SOUND_EMITTER_ACTIVE)? 0 if the library is not started.
 * PC: state[2] is the flags' N64 offset (the PC's record has them at +0x10); nothing calls this. */
unsigned char sound_emitter_is_active(unsigned int *state)
{
    if (sound_initialized) return (state[2] & SOUND_EMITTER_ACTIVE) != 0;
    return 0;
}

/* Adds an emitter: state is the caller's record (or null for a one-shot), position and velocity its vectors,
 * range and curve its volume by distance, flags the caller's SOUND_EMITTER_* flags, identifier the effect,
 * context its group, gain and minimum its volume at the emitter and at the edge of range (0..127). A one-shot
 * plays the effect once at the volume and pans computed now and returns its voice handle (-1 if out of range or
 * no voice). Otherwise the emitter joins sound_emitters, to start at the next update; returns -1. */
int sound_emitter_add(void *state, const void *position, const void *velocity, float range, float curve,
                  u32 flags, u16 identifier, u32 context, u8 gain, u8 minimum)
{
    SoundEmitterAddView *target;
    float pan, span, send, volume, pitch;
    u32 handle;
    sound_hw_lock();
    if (state == 0) target = &sound_oneshot_emitter;
    else target = state;
    target->flags = flags;
    target->position = *(const Vector *)position;
    target->velocity = *(const Vector *)velocity;
    target->range = range;
    target->identifier = identifier;
    target->gain = (float)gain / 127.0f;
    target->minimum = (float)minimum / 127.0f;
    target->curve = curve;
    target->context = context;
    if (state == 0) {
        sound_emitter_compute((StateNode *)target, &volume, &pitch, &pan, &span, &send);
        if (volume == 0.0f) {
            sound_hw_unlock();
            return -1;
        }
        handle = sound_start_sfx(target->identifier, SOUND_MIDI_MAX, SOUND_PAN_CENTER);
        target->handle = handle;
        if (handle == 0xFFFFFFFF) {
            sound_hw_unlock();
            return -1;
        }
        sound_emitter_apply((StateNode *)target, volume, pan, span, send, pitch);
        sound_hw_unlock();
        return target->handle;
    }
    if ((target->next = sound_emitters) != 0) sound_emitters->previous = (StateNode *)target;
    target->previous = 0;
    sound_emitters = (StateNode *)target;
    target->handle = 0xFFFFFFFF;
    target->counter = 0;
    target->flags |= SOUND_EMITTER_ACTIVE | SOUND_EMITTER_START;
    sound_hw_unlock();
    return -1;
}

/* sound_emitter_add with the effect's own group (its id with the top bit set): emitters of one effect share
 * their voices. Returns -1 if the library is not started. */
int sound_emitter_add_sfx(void *state, const void *position, const void *velocity, float range,
                 float curve, u32 flags, u16 identifier, u8 gain, u8 minimum)
{
    if (sound_initialized) {
        return sound_emitter_add(state, position, velocity, range, curve, flags,
                            identifier, identifier | 0x80000000, gain, minimum);
    }
    return -1;
}

/* sound_emitter_add with a group of the caller's. Returns -1 if the library is not started. */
int sound_emitter_add_with_context(void *state, const void *position, const void *velocity, float range,
                 float curve, u32 flags, u16 identifier, u16 group, u8 gain, u8 minimum)
{
    if (sound_initialized) {
        return sound_emitter_add(state, position, velocity, range, curve, flags,
                            identifier, group, gain, minimum);
    }
    return -1;
}

/* Removes an emitter (its voice keyed off). Returns 1, or 0 if the library is not started. */
int sound_emitter_remove(StateNode *state)
{
    if (sound_initialized) {
        sound_hw_lock();
        sound_emitter_unlink(state);
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* Returns an emitter's voice handle, or -1 if it is not in the list (or the library is not started). */
unsigned int sound_emitter_get_sound(StatePrefix *state)
{
    unsigned int result;
    result = 0xFFFFFFFF;
    if (sound_initialized) {
        sound_hw_lock();
        if (state->flags08 & SOUND_EMITTER_ACTIVE) result = state->identifier34;
        sound_hw_unlock();
    }
    return result;
}

/* Removes every emitter (sound_stop_everything). */
void sound_emitter_remove_all(void)
{
    StateNode *state;
    StateNode *next;
    state = sound_emitters;
    while (state != 0) {
        next = state->next;
        sound_emitter_remove(state);
        state = next;
    }
}

/* Recomputes a listener's side axis (up x forward) and the inverse of its transform (its axes and position), the
 * matrix sound_emitter_compute takes emitters into listener space with. */
void sound_listener_build_matrix(SpatialState *state)
{
    float matrix[12];
    sound_cross(&state->side, &state->up, &state->forward);
    matrix[0] = state->side.x;
    matrix[3] = state->side.y;
    matrix[6] = state->side.z;
    matrix[1] = state->up.x;
    matrix[4] = state->up.y;
    matrix[7] = state->up.z;
    matrix[2] = state->forward.x;
    matrix[5] = state->forward.y;
    matrix[8] = state->forward.z;
    matrix[9] = state->position.x;
    matrix[10] = state->position.y;
    matrix[11] = state->position.z;
    sound_invert_transform(state->inverse, matrix);
}

/* Moves a listener: position, velocity, forward and up (stored negated) vectors, and its volume (level,
 * 0..127). Returns 1, or 0 if the library is not started. */
int sound_listener_update(SoundListenerUpdateView *state, const Vector *position, const Vector *velocity,
                  const Vector *forward, const Vector *up, u8 level)
{
    if (sound_initialized) {
        sound_hw_lock();
        state->position = *position;
        state->velocity = *velocity;
        state->forward = *forward;
        state->up.x = -up->x;
        state->up.y = -up->y;
        state->up.z = -up->z;
        sound_listener_build_matrix((SpatialState *)state);
        state->gain84 = (float)level / 127.0f;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* Adds a listener (the caller's record) at the head of sound_listeners: its vectors as sound_listener_update,
 * the distances in front and behind at which sound is fully surround, the speed of sound (for doppler), its
 * SOUND_LISTENER_* flags and volume (level, 0..127). Returns 1, or 0 if the library is not started. */
int sound_listener_add(SoundListenerAddView *state, const Vector *position, const Vector *velocity,
                  const Vector *forward, const Vector *up, float surround_front, float surround_back,
                  float sound_speed, u32 flags, u8 level)
{
    if (sound_initialized) {
        sound_hw_lock();
        if ((state->next = sound_listeners) != 0) sound_listeners->previous = (LinkNode *)state;
        state->previous = 0;
        sound_listeners = (LinkNode *)state;
        state->position = *position;
        state->velocity = *velocity;
        state->forward = *forward;
        state->up.x = -up->x;
        state->up.y = -up->y;
        state->up.z = -up->z;
        state->surround_front = surround_front;
        state->surround_back = surround_back;
        state->sound_speed = sound_speed;
        sound_listener_build_matrix((SpatialState *)state);
        state->flags08 = flags;
        state->gain84 = (float)level / 127.0f;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* Takes a listener out of sound_listeners. Returns 1, or 0 if the library is not started. */
int sound_listener_remove(LinkNode *node)
{
    if (sound_initialized) {
        sound_hw_lock();
        if (node->next != 0) node->next->previous = node->previous;
        if (node->previous != 0) node->previous->next = node->next;
        else sound_listeners = node->next;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* Empties the voice limit's per-frame lists (groups, candidates, playing emitters). */
void sound_emitter_reset_groups(void)
{
    sound_emitter_group_count = 0;
    sound_emitter_candidate_count = 0;
    sound_emitter_playing_count = 0;
}

/* Notes a continuous emitter that plays, at its computed volume, in its group's list of playing emitters (kept
 * quietest first), creating the group if new. */
void sound_emitter_note_playing(StateNode *object, float volume)
{
    long i;
    SmallNode *current;
    SmallNode *previous;
    for (i = 0; i < sound_emitter_group_count; ++i) {
        if (object->group == sound_emitter_groups[i].identifier) break;
    }
    if (i == sound_emitter_group_count) {
        sound_emitter_groups[i].large = 0;
        sound_emitter_groups[i].small = 0;
        sound_emitter_groups[i].identifier = object->group;
        ++sound_emitter_group_count;
    }
    previous = 0;
    for (current = sound_emitter_groups[i].small; current != 0; current = current->next) {
        if (current->volume > volume) break;
        previous = current;
    }
    if (previous == 0) sound_emitter_groups[i].small = &sound_emitter_playing[sound_emitter_playing_count];
    else previous->next = &sound_emitter_playing[sound_emitter_playing_count];
    sound_emitter_playing[sound_emitter_playing_count].next = current;
    sound_emitter_playing[sound_emitter_playing_count].object = object;
    sound_emitter_playing[sound_emitter_playing_count++].volume = volume;
}

/* Notes a continuous emitter waiting to start, with its computed volume, pans and doppler, in its group's list of
 * candidates, creating the group if new. Returns 1, or 0 when the groups or the candidates are full (the emitter
 * then waits for another frame). */
int sound_emitter_note_candidate(StateNode *object, float volume, float x_pan, float y_pan, float z_pan, float doppler)
{
    long i;
    LargeNode *current;
    LargeNode *next;
    for (i = 0; i < sound_emitter_group_count; ++i) {
        if (object->group == sound_emitter_groups[i].identifier) break;
    }
    if (i == sound_emitter_group_count) {
        if (sound_emitter_group_count == SOUND_EMITTER_GROUP_MAX) return 0;
        sound_emitter_groups[i].large = 0;
        sound_emitter_groups[i].small = 0;
        sound_emitter_groups[i].identifier = object->group;
        ++sound_emitter_group_count;
    }
    if (sound_emitter_candidate_count == SOUND_EMITTER_GROUP_MAX) return 0;
    current = sound_emitter_groups[i].large;
    if (sound_emitter_groups[i].large != 0) {
        /* Original game: meant to keep the list loudest first, but the test is on `current`, not `next`, and the
         * new node always goes after the head: inserting 7 into 9, 5 gives 9, 5, 7. */
        next = current->next;
        while (next != 0) {
            if (current->volume < volume) break;
            current = next;
            next = next->next;
        }
        sound_emitter_candidates[sound_emitter_candidate_count].next = next;
        current->next = &sound_emitter_candidates[sound_emitter_candidate_count];
    } else {
        sound_emitter_candidates[sound_emitter_candidate_count].next = sound_emitter_groups[i].large;
        sound_emitter_groups[i].large = &sound_emitter_candidates[sound_emitter_candidate_count];
    }
    sound_emitter_candidates[sound_emitter_candidate_count].object = object; sound_emitter_candidates[sound_emitter_candidate_count].x_pan = x_pan;
    sound_emitter_candidates[sound_emitter_candidate_count].volume = volume; sound_emitter_candidates[sound_emitter_candidate_count].doppler = doppler; sound_emitter_candidates[sound_emitter_candidate_count].y_pan = y_pan; sound_emitter_candidates[sound_emitter_candidate_count++].z_pan = z_pan;
    return 1;
}

/* Starts the candidates of each group, in list order. A candidate starts when the group has no playing emitter to
 * compare with, or it is louder than the group's quietest playing one by more than the steal margin; within the
 * keep margin it waits; between the two it starts after waiting SOUND_EMITTER_STEAL_FRAMES frames. Each candidate
 * that starts moves the comparison on to the next quietest playing emitter. A candidate that gets no voice is
 * removed unless restartable; one that starts fades in. */
void sound_emitter_start_candidates(void)
{
    int i;
    EmitGroup *group;
    LargeNode *entry;
    StateNode *state;
    float difference, upper, lower;
    i = 0;
    if (sound_emitter_group_count > 0) {
        upper = sound_emitter_steal_margin;
        group = sound_emitter_groups; lower = sound_emitter_keep_margin; if (lower) {} do {
            for (entry = group->large; entry != 0; entry = entry->next) {
                if (group->small != 0) {
                    difference = entry->volume - group->small->volume;
                    if (difference <= lower) continue;
                    if (difference <= upper) {
                        entry->object->counter++;
                        if (entry->object->counter < SOUND_EMITTER_STEAL_FRAMES) continue;
                    } else entry->object->counter = 0;
                }
                state = entry->object;
                state->identifier34 = sound_start_sfx_unlocked(state->sound_id, SOUND_MIDI_MAX, SOUND_PAN_CENTER);
                if (state->identifier34 == 0xFFFFFFFFU) {
                    if (!(state->flags08 & SOUND_EMITTER_RESTARTABLE)) {
                        state->flags08 |= SOUND_EMITTER_REMOVE;
                        state->flags08 &= ~(u32)SOUND_EMITTER_START;
                    }
                } else {
                    state->fade = 0.0f;
                    state->flags08 |= SOUND_EMITTER_FADE_IN;
                    sound_emitter_apply(state, entry->volume, entry->x_pan, entry->y_pan, entry->z_pan, entry->doppler);
                    state->flags08 &= ~(u32)SOUND_EMITTER_START;
                    if (group->small != 0) group->small = group->small->next;
                }
            }
            i++; group++;
        } while (i < sound_emitter_group_count);
    }
}

/* The 3D update, once a frame (sound_hw_service_frame), under the library's lock. For each emitter: unlinks one
 * marked for removal; computes volume, pans and doppler for a continuous one or one about to start; a paused one
 * resumes (to start again) once it is back in range. Starting: a pausable emitter out of range pauses instead; a
 * continuous one becomes a candidate of its group (started by sound_emitter_start_candidates at the end), other
 * ones start now, and one that gets no voice is removed unless restartable. Playing: an ended voice is restarted
 * (restartable) or removed; a playing voice is noted in its group, stopped and paused (or removed) when a
 * pausable emitter goes out of range, else updated (sound_emitter_apply); the fade-in advances. */
void sound_emitter_update_all(void)
{
    StateNode *emitter;
    StateNode *next;
    float volume;
    float x_pan;
    float y_pan;
    float z_pan;
    float pitch;

    sound_emitter_reset_groups();
    emitter = sound_emitters;
    for (; emitter != 0; emitter = next) {
        next = emitter->next;
        if (emitter->flags08 & SOUND_EMITTER_REMOVE) {
            sound_emitter_unlink(emitter);
            continue;
        }
        /* Original game: an emitter that is neither continuous nor starting is not computed, yet a playing one is
         * still given volume, pans and pitch below: the previous emitter's in the list (unset for the first). */
        if (emitter->flags08 & (SOUND_EMITTER_START | SOUND_EMITTER_CONTINUOUS)) {
            sound_emitter_compute(emitter, &volume, &pitch, &x_pan, &y_pan, &z_pan);
        }
        if (!(emitter->flags08 & SOUND_EMITTER_PAUSED)) {
            if (emitter->flags08 & SOUND_EMITTER_START) {
                if (volume == 0.0f && (emitter->flags08 & SOUND_EMITTER_PAUSABLE)) {
                    emitter->flags08 |= SOUND_EMITTER_PAUSED;
                    emitter->flags08 &= ~SOUND_EMITTER_START;
                    goto found_emitter;
                }
                if (emitter->flags08 & SOUND_EMITTER_CONTINUOUS) {
                    if (sound_emitter_note_candidate(emitter, volume, x_pan, y_pan, z_pan, pitch)) {
                        continue;
                    }
                } else {
                    if ((emitter->identifier34 = sound_start_sfx_unlocked(emitter->sound_id, SOUND_MIDI_MAX, SOUND_PAN_CENTER)) == 0xFFFFFFFFU) {
                        if (!(emitter->flags08 & SOUND_EMITTER_RESTARTABLE)) {
                            emitter->flags08 |= SOUND_EMITTER_REMOVE;
                            emitter->flags08 &= ~SOUND_EMITTER_START;
                        } else {
                            continue;
                        }
                    }
                }
            } else if ((emitter->identifier34 = sound_check(emitter->identifier34)) == 0xFFFFFFFFU) {
                if (emitter->flags08 & SOUND_EMITTER_RESTARTABLE) {
                    emitter->flags08 |= SOUND_EMITTER_START;
                } else {
                    emitter->flags08 |= SOUND_EMITTER_REMOVE;
                }
            }
found_emitter:
            if (emitter->identifier34 != 0xFFFFFFFFU) {
                if (emitter->flags08 & SOUND_EMITTER_CONTINUOUS) {
                    sound_emitter_note_playing(emitter, volume);
                }
                if (volume == 0.0f && (emitter->flags08 & SOUND_EMITTER_PAUSABLE)) {
                    sound_key_off_unlocked(emitter->identifier34);
                    emitter->identifier34 = 0xFFFFFFFFU;
                    if (emitter->flags08 & SOUND_EMITTER_RESTARTABLE) {
                        emitter->flags08 |= SOUND_EMITTER_PAUSED;
                    } else {
                        emitter->flags08 |= SOUND_EMITTER_REMOVE;
                    }
                } else {
                    sound_emitter_apply(emitter, volume, x_pan, y_pan, z_pan, pitch);
                }
            }
            if (emitter->flags08 & SOUND_EMITTER_FADE_IN) {
                emitter->fade += sound_emitter_fade_step;
                if (emitter->fade >= 1.0f) {
                    emitter->flags08 &= ~SOUND_EMITTER_FADE_IN;
                }
            }
        } else if (volume != 0.0f) {
            emitter->flags08 &= ~SOUND_EMITTER_PAUSED;
            emitter->flags08 |= SOUND_EMITTER_START;
        }
    }
    sound_emitter_start_candidates();
}

/* Starts the 3D sound: no emitters, no listeners. */
void sound_emitter_init(void)
{
    sound_emitters = 0;
    sound_listeners = 0;
}

/* Stops the 3D sound: nothing to do. */
void sound_emitter_exit(void)
{
}
