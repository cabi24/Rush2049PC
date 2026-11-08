/* GENERATED ROM-aligned TU — segment 0x1cf90 (rom/lib_1cf90)
 * layout map 49b03b3d75a02558e10ab39c8693394c31f03cc7f18aec15a71756604b34fc1e.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "../rom_tu.h"

/* PROMOTED 2026-10-11 — sound_stream_reset_all
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern unsigned char sound_voice_count;
/* On the PC a voice's stream record is PC data, PC_MX_VOICE_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9): a stream plays on the voice sound_alloc_stream_voice gives it, so its record is that voice's. */
#define sound_streams (*(SampleBuffer (*)[PC_MX_VOICE_SLOTS])pc_mx_streams)
void sound_stream_reset_all(void)
{
    int i;
    for (i = 0; i < sound_voice_count; i++) {
        sound_streams[i].mode = 0;
    }
}

/* PROMOTED 2026-10-04 — sound_stream_refill
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern u8 sound_voice_count;
extern u32 sound_hw_get_voice_position(int);
extern void sound_hw_flush_stream(short *, u32);
void sound_stream_refill(void)
{
    int i;
    u32 position;
    for (i = 0; i < sound_voice_count; i++) {
        if (sound_streams[i].mode == 1) {
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

/* PROMOTED 2026-10-05 — sound_stream_flush_wraps
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern unsigned char sound_voice_count;
extern void sound_hw_flush_stream_wrap(short *, unsigned int);
void sound_stream_flush_wraps(void)
{
    int i;
    for (i = 0; i < sound_voice_count; i++) {
        if (sound_streams[i].mode == 1) {
            sound_hw_flush_stream_wrap(sound_streams[i].buffer, sound_streams[i].samples);
        }
    }
}

/* PROMOTED 2026-10-11 — sound_stream_start
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#pragma pack(1)
typedef struct SoundHwSampleInfo { unsigned int frequency; void *data; unsigned int offset; unsigned int length; unsigned int loopStart; unsigned int loopLength; unsigned char format; } SoundHwSampleInfo;
#pragma pack()
extern unsigned char sound_initialized;
extern int sound_synth_frequency;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern int sound_alloc_stream_voice(unsigned char);
extern void sound_hw_set_voice_sample(unsigned int, SoundHwSampleInfo *, unsigned char);
extern void sound_hw_set_pitch(int, unsigned short);
extern void sound_hw_set_volume(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
extern void sound_hw_start_voice(unsigned int);
int sound_stream_start(unsigned char request, short *buffer, unsigned int samples, unsigned int frequency,
                  unsigned char volume, unsigned char pan, unsigned char span, unsigned char aux,
                  SampleCallback callback, unsigned int context)
{
    int index;
    SoundHwSampleInfo info;
    float ratio;
    if (sound_initialized) {
        sound_hw_lock();
        index = sound_alloc_stream_voice(request);
        if (index != -1) {
            sound_streams[index].callback = callback;
            sound_streams[index].mode = 1;
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

/* PROMOTED 2026-10-04 — sound_stream_buffer_size
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned int sound_stream_buffer_size(unsigned int count)
{
    return count * 2 + 8;
}

/* PROMOTED 2026-10-11 — sound_stream_set_volume
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern void sound_hw_set_volume(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
void sound_stream_set_volume(unsigned int channel, unsigned char first, unsigned char second, unsigned char third, unsigned char fourth)
{
    if (sound_initialized && channel != 0xFFFFFFFF) {
        sound_hw_lock();
        sound_hw_set_volume(channel, first << 16, second << 16, third << 16, fourth << 16);
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-05 — sound_stream_stop
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern unsigned char sound_initialized;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
extern void sound_free_stream_voice(unsigned int);
void sound_stream_stop(unsigned int channel)
{
    if (sound_initialized) {
        sound_hw_lock();
        if (sound_streams[channel].mode == 1) {
            sound_streams[channel].mode = 0;
            sound_free_stream_voice(channel);
        }
        sound_hw_unlock();
    }
}

/* PROMOTED 2026-10-11 — sound_emitter_compute
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
typedef struct SoundVector { float x, y, z; } SoundVector;
typedef struct SoundEmitter { StateNode *next, *previous; u32 flags; SoundVector pos, dir; float maxDis, maxVol, minVol, volPush; } SoundEmitter;
typedef struct SoundListener { struct SoundListener *next, *previous; u32 flags; SoundVector pos, dir, heading, right, up; float mat[12]; float surroundDisFront, surroundDisBack, soundSpeed, vol; } SoundListener;
extern LinkNode *sound_listeners;
extern float sqrtf(float);
extern void sound_transform_point(const void *, const void *, void *);
extern float sound_normalize(void *);
#define EM ((SoundEmitter *)em)
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
            if (!(EM->flags & 0x80000)) {
                if ((EM->flags & 0x8) || (li->flags & 1)) {
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

/* PROMOTED 2026-10-11 — sound_clamp_midi_value
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned char sound_clamp_midi_value(unsigned char value)
{ if (value > 127) return 127; return value; }

/* PROMOTED 2026-10-04 — sound_clamp_pitch_value
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned short sound_clamp_pitch_value(unsigned int value)
{
    if (value > 16383) {
        return 16383;
    }
    return value;
}

/* PROMOTED 2026-10-11 — sound_emitter_apply
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_clamp_midi_value(u8);
extern u16 sound_clamp_pitch_value(u32);
extern int sound_set_sfx_volume_unlocked(u32, u8);
extern int sound_set_sfx_pan_unlocked(u32, u8);
extern int sound_set_sfx_span_unlocked(u32, u8);
extern int sound_set_sfx_doppler_unlocked(u32, u16);
/* PC: IDO's float to u32 (pc_prelude.h), here also the implicit one of the doppler factor, which is negative once
 * a source closes in faster than sound: 0xFFFFFFFF, the top pitch, on the N64 and x86-64, 0 on AArch64. */
#define F2U_ARG(x) pc_mips_f2u(x)
void sound_emitter_apply(StateNode *em, float vol, float xPan, float yPan,
                   float zPan, float doppler)
{
    u32 identifier;
    identifier = em->identifier34;
    if (em->flags08 & 0x100000) {
        sound_set_sfx_volume_unlocked(identifier, sound_clamp_midi_value(N64_F2U((em->fade * vol) * 127.0f)));
    } else {
        sound_set_sfx_volume_unlocked(identifier, sound_clamp_midi_value(N64_F2U(vol * 127.0f)));
    }
    sound_set_sfx_pan_unlocked(identifier, sound_clamp_midi_value(N64_F2U((1.0f + xPan) * 64.0f)));
    sound_set_sfx_span_unlocked(identifier, sound_clamp_midi_value(N64_F2U((1.0f - zPan) * 64.0f)));
    sound_set_sfx_doppler_unlocked(identifier, sound_clamp_pitch_value(F2U_ARG(doppler * 8192.0f)));
}

/* PROMOTED 2026-10-04 — sound_emitter_unlink
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern StateNode *sound_emitters;
extern int sound_key_off_unlocked(u32);
void sound_emitter_unlink(StateNode *state)
{
    if (state->next != 0) state->next->previous = state->previous;
    if (state->previous != 0) state->previous->next = state->next;
    else sound_emitters = state->next;
    state->flags08 &= 0xFFFF;
    if (state->identifier34 != 0xFFFFFFFF) sound_key_off_unlocked(state->identifier34);
}

typedef struct Vector { float x, y, z; } Vector;
/* PROMOTED 2026-10-11 — sound_emitter_update
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SoundEmitterUpdateView { u8 unknown00[12]; Vector position; Vector velocity; unsigned int unknown24; float gain28; float current2C; } SoundEmitterUpdateView;
extern u8 sound_initialized;
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

/* PROMOTED 2026-10-11 — sound_emitter_is_active
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
unsigned char sound_emitter_is_active(unsigned int *state)
{
    if (sound_initialized) return (state[2] & 0x10000) != 0;
    return 0;
}

/* PROMOTED 2026-10-11 — sound_emitter_add
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "types.h"
typedef struct SoundEmitterAddView { StateNode *next, *previous; u32 flags; Vector position, velocity; float range, gain, minimum, curve; u32 handle, context; u16 identifier, counter; float fade; } SoundEmitterAddView;
extern SoundEmitterAddView sound_oneshot_emitter;
extern void sound_emitter_compute(StateNode *, float *, float *, float *, float *, float *);
extern void sound_emitter_apply(StateNode *em, float vol, float xPan, float yPan, float zPan, float doppler);
extern u32 sound_start_sfx(u16, u8, u8);
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
        handle = sound_start_sfx(target->identifier, 127, 64);
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
    target->flags |= 0x30000;
    sound_hw_unlock();
    return -1;
}

/* PROMOTED 2026-10-04 — sound_emitter_add_sfx
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern u8 sound_initialized;
extern int sound_emitter_add(void *, const void *, const void *, float, float, u32, u16, u32, u8, u8);
int sound_emitter_add_sfx(void *state, const void *first, const void *second, float fourth,
                 float fifth, u32 flags, u16 identifier, u8 eighth, u8 ninth)
{
    if (sound_initialized) {
        return sound_emitter_add(state, first, second, fourth, fifth, flags,
                            identifier, identifier | 0x80000000, eighth, ninth);
    }
    return -1;
}

/* PROMOTED 2026-10-04 — sound_emitter_add_with_context
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_emitter_add_with_context(void *state, const void *first, const void *second, float fourth,
                 float fifth, u32 flags, u16 identifier, u16 eighth, u8 ninth, u8 tenth)
{
    if (sound_initialized) {
        return sound_emitter_add(state, first, second, fourth, fifth, flags,
                            identifier, eighth, ninth, tenth);
    }
    return -1;
}

/* PROMOTED 2026-10-05 — sound_emitter_remove
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern void sound_emitter_unlink(StateNode *);
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

/* PROMOTED 2026-10-04 — sound_emitter_get_sound
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_initialized;
extern void sound_hw_lock(void);
extern void sound_hw_unlock(void);
unsigned int sound_emitter_get_sound(StatePrefix *state)
{
    unsigned int result;
    result = 0xFFFFFFFF;
    if (sound_initialized) {
        sound_hw_lock();
        if (state->flags08 & 0x10000) result = state->identifier34;
        sound_hw_unlock();
    }
    return result;
}

/* PROMOTED 2026-10-04 — sound_emitter_remove_all
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int sound_emitter_remove(StateNode *);
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

/* PROMOTED 2026-10-04 — sound_listener_build_matrix
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: a listener (SoundListener, sound_listener_add's view) has two host-sized links before its flags, so its vectors
 * start at +0x14, not +0xC. */
typedef struct SpatialState { LinkNode *next, *previous; u32 flags08; Vector position; unsigned char unknown18[12]; Vector forward; Vector side; Vector up; float inverse[12]; } SpatialState;
extern void sound_cross(Vector *, const Vector *, const Vector *);
extern void sound_invert_transform(float *, const float *);
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

/* PROMOTED 2026-10-11 — sound_listener_update
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
/* PC: the same listener, as sound_listener_add links it (SpatialState above). */
typedef struct SoundListenerUpdateView { LinkNode *next, *previous; u32 flags08; Vector position; Vector velocity; Vector forward; Vector side; Vector up; float inverse[12]; u8 unknown78[12]; float gain84; } SoundListenerUpdateView;
extern void sound_listener_build_matrix(SpatialState *);
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

/* PROMOTED 2026-10-11 — sound_listener_add
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SoundListenerAddView { LinkNode *next, *previous; u32 flags08; Vector position, velocity, forward, side, up; float inverse[12]; float first78, second7C, third80, gain84; } SoundListenerAddView;
extern LinkNode *sound_listeners;
int sound_listener_add(SoundListenerAddView *state, const Vector *position, const Vector *velocity,
                  const Vector *forward, const Vector *up, float first, float second,
                  float third, u32 flags, u8 level)
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
        state->first78 = first;
        state->second7C = second;
        state->third80 = third;
        sound_listener_build_matrix((SpatialState *)state);
        state->flags08 = flags;
        state->gain84 = (float)level / 127.0f;
        sound_hw_unlock();
        return 1;
    }
    return 0;
}

/* PROMOTED 2026-10-04 — sound_listener_remove
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern LinkNode *sound_listeners;
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

/* PROMOTED 2026-10-04 — sound_emitter_reset_groups
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern unsigned char sound_emitter_group_count, sound_emitter_candidate_count, sound_emitter_playing_count;
void sound_emitter_reset_groups(void)
{
    sound_emitter_group_count = 0;
    sound_emitter_candidate_count = 0;
    sound_emitter_playing_count = 0;
}

/* PROMOTED 2026-10-06 — sound_emitter_note_playing
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef struct SmallNode { struct SmallNode *next; float distance; StateNode *object; } SmallNode;
typedef struct LargeNode { struct LargeNode *next; float distance, first, second, third, fourth; StateNode *object; } LargeNode;
typedef struct EmitGroup { unsigned int identifier; LargeNode *large; SmallNode *small; } EmitGroup;
extern EmitGroup sound_emitter_groups[32];
extern unsigned char sound_emitter_group_count;
extern LargeNode sound_emitter_candidates[32];
extern unsigned char sound_emitter_candidate_count;
extern SmallNode sound_emitter_playing[32];
extern unsigned char sound_emitter_playing_count;
void sound_emitter_note_playing(StateNode *object, float distance)
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
        if (current->distance > distance) break;
        previous = current;
    }
    if (previous == 0) sound_emitter_groups[i].small = &sound_emitter_playing[sound_emitter_playing_count];
    else previous->next = &sound_emitter_playing[sound_emitter_playing_count];
    sound_emitter_playing[sound_emitter_playing_count].next = current;
    sound_emitter_playing[sound_emitter_playing_count].object = object;
    sound_emitter_playing[sound_emitter_playing_count++].distance = distance;
}

/* PROMOTED 2026-10-11 — sound_emitter_note_candidate
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
int sound_emitter_note_candidate(StateNode *object, float distance, float first, float second, float third, float fourth)
{
    long i;
    LargeNode *current;
    LargeNode *next;
    for (i = 0; i < sound_emitter_group_count; ++i) {
        if (object->group == sound_emitter_groups[i].identifier) break;
    }
    if (i == sound_emitter_group_count) {
        if (sound_emitter_group_count == 32) return 0;
        sound_emitter_groups[i].large = 0;
        sound_emitter_groups[i].small = 0;
        sound_emitter_groups[i].identifier = object->group;
        ++sound_emitter_group_count;
    }
    if (sound_emitter_candidate_count == 32) return 0;
    current = sound_emitter_groups[i].large;
    if (sound_emitter_groups[i].large != 0) {
        next = current->next;
        while (next != 0) {
            if (current->distance < distance) break;
            current = next;
            next = next->next;
        }
        sound_emitter_candidates[sound_emitter_candidate_count].next = next;
        current->next = &sound_emitter_candidates[sound_emitter_candidate_count];
    } else {
        sound_emitter_candidates[sound_emitter_candidate_count].next = sound_emitter_groups[i].large;
        sound_emitter_groups[i].large = &sound_emitter_candidates[sound_emitter_candidate_count];
    }
    sound_emitter_candidates[sound_emitter_candidate_count].object = object; sound_emitter_candidates[sound_emitter_candidate_count].first = first;
    sound_emitter_candidates[sound_emitter_candidate_count].distance = distance; sound_emitter_candidates[sound_emitter_candidate_count].fourth = fourth; sound_emitter_candidates[sound_emitter_candidate_count].second = second; sound_emitter_candidates[sound_emitter_candidate_count++].third = third;
    return 1;
}

/* PROMOTED 2026-10-11 — sound_emitter_start_candidates
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern float sound_emitter_steal_margin, sound_emitter_keep_margin;
extern unsigned int sound_start_sfx_unlocked(unsigned short, unsigned char, unsigned char);
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
                    difference = entry->distance - group->small->distance;
                    if (difference <= lower) continue;
                    if (difference <= upper) {
                        entry->object->counter++;
                        if (entry->object->counter < 20) continue;
                    } else entry->object->counter = 0;
                }
                state = entry->object;
                state->identifier34 = sound_start_sfx_unlocked(state->sound_id, 127, 64);
                if (state->identifier34 == 0xFFFFFFFFU) {
                    if (!(state->flags08 & 2)) {
                        state->flags08 |= 0x40000;
                        state->flags08 &= ~0x20000U;
                    }
                } else {
                    state->fade = 0.0f;
                    state->flags08 |= 0x100000;
                    sound_emitter_apply(state, entry->distance, entry->first, entry->second, entry->third, entry->fourth);
                    state->flags08 &= ~0x20000U;
                    if (group->small != 0) group->small = group->small->next;
                }
            }
            i++; group++;
        } while (i < sound_emitter_group_count);
    }
}

/* PROMOTED 2026-10-05 — sound_emitter_update_all
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
extern const float sound_emitter_fade_step;
extern void sound_emitter_reset_groups(void);
extern void sound_emitter_compute(StateNode *, float *, float *, float *, float *, float *);
extern int sound_emitter_note_candidate(StateNode *, float, float, float, float, float);
extern unsigned int sound_start_sfx_unlocked(unsigned short, unsigned char, unsigned char);
extern unsigned int sound_check(unsigned int);
extern void sound_emitter_note_playing(StateNode *, float);
extern int sound_key_off_unlocked(unsigned int);
extern void sound_emitter_apply(StateNode *, float, float, float, float, float);
extern void sound_emitter_start_candidates(void);
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
        if (emitter->flags08 & 0x40000) {
            sound_emitter_unlink(emitter);
            continue;
        }
        if (emitter->flags08 & 0x20001) {
            sound_emitter_compute(emitter, &volume, &pitch, &x_pan, &y_pan, &z_pan);
        }
        if (!(emitter->flags08 & 0x80000)) {
            if (emitter->flags08 & 0x20000) {
                if (volume == 0.0f && (emitter->flags08 & 4)) {
                    emitter->flags08 |= 0x80000;
                    emitter->flags08 &= ~0x20000;
                    goto found_emitter;
                }
                if (emitter->flags08 & 1) {
                    if (sound_emitter_note_candidate(emitter, volume, x_pan, y_pan, z_pan, pitch)) {
                        continue;
                    }
                } else {
                    if ((emitter->identifier34 = sound_start_sfx_unlocked(emitter->sound_id, 127, 64)) == 0xFFFFFFFFU) {
                        if (!(emitter->flags08 & 2)) {
                            emitter->flags08 |= 0x40000;
                            emitter->flags08 &= ~0x20000;
                        } else {
                            continue;
                        }
                    }
                }
            } else if ((emitter->identifier34 = sound_check(emitter->identifier34)) == 0xFFFFFFFFU) {
                if (emitter->flags08 & 2) {
                    emitter->flags08 |= 0x20000;
                } else {
                    emitter->flags08 |= 0x40000;
                }
            }
found_emitter:
            if (emitter->identifier34 != 0xFFFFFFFFU) {
                if (emitter->flags08 & 1) {
                    sound_emitter_note_playing(emitter, volume);
                }
                if (volume == 0.0f && (emitter->flags08 & 4)) {
                    sound_key_off_unlocked(emitter->identifier34);
                    emitter->identifier34 = 0xFFFFFFFFU;
                    if (emitter->flags08 & 2) {
                        emitter->flags08 |= 0x80000;
                    } else {
                        emitter->flags08 |= 0x40000;
                    }
                } else {
                    sound_emitter_apply(emitter, volume, x_pan, y_pan, z_pan, pitch);
                }
            }
            if (emitter->flags08 & 0x100000) {
                emitter->fade += sound_emitter_fade_step;
                if (emitter->fade >= 1.0f) {
                    emitter->flags08 &= ~0x100000;
                }
            }
        } else if (volume != 0.0f) {
            emitter->flags08 &= ~0x80000;
            emitter->flags08 |= 0x20000;
        }
    }
    sound_emitter_start_candidates();
}

/* PROMOTED 2026-10-05 — sound_emitter_init
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#include "sound_sample_record.h"
void sound_emitter_init(void)
{
    sound_emitters = 0;
    sound_listeners = 0;
}

/* PROMOTED 2026-10-11 — sound_emitter_exit
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void sound_emitter_exit(void)
{
}

