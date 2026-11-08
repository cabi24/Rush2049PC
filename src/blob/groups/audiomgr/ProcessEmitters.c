/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* ProcessEmitters (0x80098AE4, 1236 B) + clamp_gain (0x80098ADC, locked 2-word stub given a body).
 * : EQUAL in the whole-program unit with audio_effect_setup_b132.c (this group):
 * the whole-program unit --tag score ProcessEmitters FreeFxTag clamp_gain --with audio_effect_setup_b132.c
 *     --with ProcessEmitters.c --internal FreeFxTag --internal clamp_gain --neighbours
 * PACKAGING: audio_lock/audio_unlock (locked jr-ra stubs of heap_release, the slot-queue lock and
 *   unlock) are defined again at the END of this file as group context, same bodies as the locked copies. Their
 *   position sets the source line of the inlined `la a0,audio_lock_queue`, which as1 uses to break scheduling ties: as
 *   separate files (73 "audio_lock.c" line 14) the lui stays after the register saves (16 words differ in
 * the scorer); defined before the function it is hoisted above `sw ra`; after the function it lands at +8
 *   as in retail (and as in the whole-program unit, where the inlined code carries the call-site line).
 * Semantics: per frame, under the slot-queue lock (inlined audio_lock/AF8), walk the active sound sources
 * (active_emitters_head); an effect of kind 2 that is not persistent, not delayed and whose voice has finished is released
 * (FreeFxTag); otherwise, for every listener (gActiveListeners list, at most 5), project source - listener onto
 * the listener's basis (forward, right and their arcade crossprod), compute a distance-attenuated gain
 * (near - (near - far) * d / radius, clamped at 0) and accumulate gain-weighted pan/depth; then send gain (clamped
 * 0..1 by clamp_gain), pan, depth (clamped -1..1) and the source parameter to AudChangeEffect.
 * History: drafts (see for the out[4] array, kind local, clamp helper levers).
 * levers (each measured in the unit):
 *  - cross product in the arcade vecmath.h crossprod(a,b,r) operand order (a = basis[1], b = basis[2]);
 *  - projection written basis-first (`basis[k][j]*direction[j]`), as the arcade BodyVector-style dot products;
 *  - listener loop as a for loop (`listener = listener->next, i++` in the increment), `weight = gains[i]` as the
 *    pan multiplier (a plain variable is the second operand) and `total += gains[i]`.
 * HYPOTHESIS (helper identity): clamp_gain = the inlined gain clamp writing through a pointer.
 * SHAPING (disclosed; identity unknown, both compile to no code):
 *  - `if (gains[i] != 0.0f) {}` before the radius test: an early code-free read of gains[i] that numbers the
 *    gains[i] reload expression (PRE bit) before the nearVolume load, so the reload web wins the save-300 tie
 *    against nearVolume and takes $f2 (retail), nearVolume $f12 (retail);
 * - `if (i == 5 || gActiveListeners.count == 1) {}` at the end of the listener loop: code-free uses that give
 *    retail's s5 = &gActiveListeners, s8 = 1 and the unsaved array bases.
 *  An assert/debug macro that compiles to nothing is the natural candidate for both.
 * UNUSED LOCAL (exact frame residual): pad3, 1 slot at sp+252 between basis and panValues. Retail frame 376;
 *   without it the frame still rounds to 376 but panValues/depthValues/gains move up 4 bytes (6 words differ).
 */
typedef signed char s8; typedef unsigned char u8;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
/* PC: an fx tag (audiomgr_thread.c's tFxSequence) is 88 bytes, its links and list host-sized (docs/port/SYNTH.md). */
typedef struct Effect {u8 pad0[28];s32 kind;u8 pad32[4];s8 state,delay;u8 persistent;u8 pad39[33];s32 voice;void *data;} Effect;
typedef struct Source {struct Source *next,*prev;u32 flags;f32 position[3];f32 radius,nearVolume,farVolume,parameter;s32 handle;} Source;
typedef struct Listener {struct Listener *next,*prev;u32 flags;f32 position[3];u8 pad24[12];f32 right[3],forward[3];} Listener;
typedef struct List {u8 flags[4];s32 count;Listener *head,*tail;} List;
extern List gActiveListeners;
extern Source *active_emitters_head;
extern u8 audio_lock_queue[];
void FreeFxTag(Effect *);
Effect *ConvertTag(s32);
s32 AudioEffectPlaying(s32);
f32 normalize_vector_or_x(f32 *);
void AudChangeEffect(s32,f32,f32,f32,f32);
s32 osRecvMesg(void *,void *,s32);
s32 osSendMesg(void *,void *,s32);
void audio_lock(void);
void audio_unlock(void);

/* PC: [audio] engines (port/config/voices.c, docs/port/AUDIO.md 10). Before the pass, each drone engine layer's
 * distance to each listener goes to pc_engines_note (the voices module knows which emitters are the drones' engines),
 * which then picks, per listener, the drones whose engines play; the pass sends each emitter's gain through
 * pc_engine_gain, the chosen ones' as the game computed it, the others' faded to 0. At original nothing is gathered
 * and every gain is the game's. */
static void pc_engines_gather(void)
{
    Source *source;
    Listener *listener;
    f32 direction[3];
    s32 i;
    if (!pc_engines_choosing())
        return;
    for (source = active_emitters_head; source; source = source->next) {
        for (i = 0, listener = gActiveListeners.head; listener; listener = listener->next, i++) {
            direction[0] = source->position[0] - listener->position[0];
            direction[1] = source->position[1] - listener->position[1];
            direction[2] = source->position[2] - listener->position[2];
            pc_engines_note(source->handle, i, normalize_vector_or_x(direction));
        }
    }
    pc_engines_choose(gActiveListeners.count);
}
#define PC_ENGINE_GAIN(source, gain) pc_engine_gain((source)->handle, gain)
void clamp_gain(f32 *d, f32 v)
{
    *d = v<0.0f?0.0f:v>1.0f?1.0f:v;
}

void ProcessEmitters(void)
{
    Source *source, *next;
    Effect *effect;
    f32 out[4];
    Listener *listener;
    f32 distance, total, totalPan, totalDepth, weight;
    s32 i, kind;
    f32 direction[3], projected[3], basis[3][3];
    s32 pad3;
    f32 panValues[5], depthValues[5], gains[5];

    audio_lock(); pc_engines_gather();
    source=active_emitters_head;
    while (source) {
        next=source->next;
        effect=ConvertTag(source->handle);
        kind=effect->kind; pc_engine_state(source->handle, kind);
        if (kind==2 && !effect->persistent && !effect->delay &&
            !AudioEffectPlaying(effect->voice)) {
            FreeFxTag(effect);
        } else if (!effect->state) {
            i=0;
            total=0.0f;
            totalPan=0.0f;
            totalDepth=0.0f;
            out[0]=source->parameter;
            for (listener=gActiveListeners.head; listener; listener=listener->next, i++) {
                direction[0]=source->position[0]-listener->position[0];
                direction[1]=source->position[1]-listener->position[1];
                direction[2]=source->position[2]-listener->position[2];
                distance=normalize_vector_or_x(direction);
                basis[2][0]=listener->right[0];
                basis[2][1]=listener->right[1];
                basis[2][2]=listener->right[2];
                basis[1][0]=listener->forward[0];
                basis[1][1]=listener->forward[1];
                basis[1][2]=listener->forward[2];
                basis[0][0]=basis[1][1]*basis[2][2]-basis[1][2]*basis[2][1];
                basis[0][1]=basis[1][2]*basis[2][0]-basis[1][0]*basis[2][2];
                basis[0][2]=basis[1][0]*basis[2][1]-basis[1][1]*basis[2][0];
                projected[0]=basis[0][0]*direction[0]+basis[0][1]*direction[1]+basis[0][2]*direction[2];
                projected[1]=basis[1][0]*direction[0]+basis[1][1]*direction[1]+basis[1][2]*direction[2];
                projected[2]=basis[2][0]*direction[0]+basis[2][1]*direction[1]+basis[2][2]*direction[2];
                panValues[i]=projected[0];
                depthValues[i]=projected[2];
                if (gains[i] != 0.0f) {}
                if (source->radius>0.0f) {
                    gains[i]=source->nearVolume-(source->nearVolume-source->farVolume)*(distance/source->radius);
                    if (gains[i]<0.0f) gains[i]=0.0f;
                } else gains[i]=source->nearVolume;
                weight=gains[i];
                total+=gains[i];
                panValues[i] = panValues[i] - (panValues[i] * weight);
                if (i == 5 || gActiveListeners.count == 1) {}
            }
            clamp_gain(&out[3],total);
            for (i=0; i<gActiveListeners.count; i++) {
                if (total!=0.0f) {
                    totalPan+=panValues[i]*(gains[i]/total);
                    totalDepth+=depthValues[i]*(gains[i]/total);
                }
            }
            out[2]=totalPan < -1.0f ? -1.0f : totalPan>1.0f ? 1.0f : totalPan;
            out[1]=totalDepth < -1.0f ? -1.0f : totalDepth>1.0f ? 1.0f : totalDepth;
            AudChangeEffect(source->handle,PC_ENGINE_GAIN(source, out[3]),out[2],out[1],out[0]);
        }
        source=next;
    }
    audio_unlock();
}

void audio_lock(void);

void audio_unlock(void);
